#include <kernel.h>
#include <module.h>
#include <elf.h>
#include <symbol.h>
#include <file.h>
int process_elf64_phdr(char *mem, int fp, addr_t *start, addr_t *end)
{
	uint32_t i, x;
	addr_t entry;
	elf_header_t *eh = (elf_header_t *)mem;
	char buffer[(eh->phnum+1)*eh->phsize];
	fs_read_file_data(fp, buffer, eh->phoff, eh->phsize * eh->phnum);
	uint64_t vaddr=0, length=0, offset=0, stop, tmp;
	uint64_t max=0, min=~0;
	struct file *file = fs_get_file_pointer((task_t *)current_task, fp);
	for(i=0;i < eh->phnum;i++)
	{
		elf64_program_header_t *ph = (elf64_program_header_t *)(buffer + (i*eh->phsize));
		vaddr = ph->p_addr;
		if(vaddr < min)
			min = vaddr;
		offset = ph->p_offset;
		length = ph->p_filesz;
		stop = vaddr+ph->p_memsz;
		tmp = vaddr&PAGE_MASK;
		if(ph->p_type == PH_LOAD) {
			if(stop > max) max = stop;
			while(tmp <= stop + PAGE_SIZE)
			{
				user_map_if_not_mapped(tmp);
				tmp += PAGE_SIZE;
			}
			if((unsigned)fs_do_sys_read_flags(file, offset, (char *)vaddr, length) != length) {
				fs_fput((task_t *)current_task, fp, 0);
				return 0;
			}
		}
	}
	fs_fput((task_t *)current_task, fp, 0);
	if(!max)
		return 0;
	*start = eh->entry;
	*end = max;
	return 1;
}

int process_elf(char *mem, int fp, addr_t *start, addr_t *end)
{
	return process_elf64_phdr(mem, fp, start, end);
}
int arch_loader_process_elf32_phdr(char *mem, int fp, addr_t *start, addr_t *end);
int process_elf_other(char *mem, int fp, addr_t *start, addr_t *end)
{
	return arch_loader_process_elf32_phdr(mem, fp, start, end);
}

#if (CONFIG_MODULES)

void elf64_write_field(int type, addr_t mem_addr, addr_t reloc_addr)
{
	switch(type)
	{
		case R_X86_64_32: case R_X86_64_PC32:
			*(uint32_t*)mem_addr = (uint32_t)reloc_addr;
			break;
		default:
			*(uint64_t*)mem_addr = reloc_addr;
			break;
	}
}

int arch_loader_loader_parse_elf_module(uint8_t * buf, addr_t *entry, addr_t *tm_exiter, addr_t *deps)
{
	uint32_t i, x;
	uint64_t module_entry=0, reloc_addr, mem_addr, module_exiter=0, module_deps=0;
	elf_header_t * eh;
	elf64_section_header_t * sh;
	elf64_rel_t * reloc;
	elf64_rela_t * rela;
	elf64_symtab_entry_t * symtab;
	int error=0;
	eh = (elf_header_t *)buf;
	
	/* grab the functions we'll need */
	for(i = 0; i < eh->shnum; i++)
	{  
		sh = (elf64_section_header_t*)(buf + eh->shoff + (i * eh->shsize));
		if(sh->type == 2)
		{
			for(x = 0; x < sh->size; x += sh->sect_size)
			{
				symtab = (elf64_symtab_entry_t*)(buf + sh->offset + x);
				if(!memcmp((uint8_t*)get_symbol_string(buf, symtab->name), 
						(uint8_t*)"module_install", 14))
					module_entry = get_section_offset(buf, symtab->shndx) + 
						symtab->address + (uint64_t)buf;
				if(!memcmp((uint8_t*)get_symbol_string(buf, symtab->name), 
						(uint8_t*)"module_tm_exit", 11))
					module_exiter = get_section_offset(buf, symtab->shndx) + 
						symtab->address + (uint64_t)buf;
				if(!memcmp((uint8_t*)get_symbol_string(buf, symtab->name), 
						(uint8_t*)"module_deps", 11))
					module_deps = get_section_offset(buf, symtab->shndx) + 
						symtab->address + (uint64_t)buf;
			}
		}
	}
	
	if(!module_entry)
	{
		printk(KERN_INFO, "[mod]: module_install() entry point was not found\n");
		return 1;
	}
	
	*entry = module_entry;
	*tm_exiter = module_exiter;
	*deps = module_deps;
	
	/* fix up the relocation entries */
	for(i = 0; i < eh->shnum; i++)
	{  
		sh = (elf64_section_header_t*)(buf + eh->shoff + (i * eh->shsize));
		/* 64-bit ELF only deals in rela relocation sections */
		if(sh->type == 4) {
			for(x = 0; x < sh->size; x += sh->sect_size)
			{
				rela = (elf64_rela_t*)(buf + sh->offset + x);
				symtab = fill_symbol_struct(buf, GET_RELOC_SYM(rela->info));
				mem_addr = (uint64_t)buf + rela->offset;
				mem_addr += get_section_offset(buf, sh->info);
				
				reloc_addr = (uint64_t)buf + symtab->address;
				reloc_addr += get_section_offset(buf, symtab->shndx);
				if(symtab->shndx == 0)
				{
					reloc_addr = loader_find_kernel_function(get_symbol_string(buf, symtab->name));
					if(!reloc_addr)
					{
						printk(KERN_INFO, "[mod]: %x: unresolved dependency \"%s\"\n", 
								rela->info, get_symbol_string(buf, symtab->name));
						error++;
					}
				} else {
					if(GET_RELOC_TYPE(rela->info) == R_X86_64_64)
						reloc_addr += *(uint64_t *)(mem_addr) + rela->addend;
					else if(GET_RELOC_TYPE(rela->info) == R_X86_64_32)
						reloc_addr += *(uint64_t *)(mem_addr) + rela->addend;
					else
					{
						printk(KERN_INFO, "[mod]: invalid relocation type (%x)\n", 
								GET_RELOC_TYPE(rela->info));
						error++;
					}
				}
				elf64_write_field(GET_RELOC_TYPE(rela->info), mem_addr, reloc_addr);
			}
		}
	}
	return error;
}
#endif
