#include <nds.h>
#include "vector.h"
#include "vram.h"
#include "vramheap.h"
#include "sd_access.h"
#include "fat.h"


PUT_IN_VRAM int strlen( char* str )
{
    char* ptr = str;
    while ( *ptr != '\0' )
        ++ptr;
    return ptr - str;
}

PUT_IN_VRAM char *strchr(const char *s, int c)
{
    while (*s != (char)c)
        if (!*s++)
            return 0;
    return (char *)s;
}

PUT_IN_VRAM char *strrchr(const char *s, int c)
{
    char* ret=0;
    do {
        if( *s == (char)c )
            ret=(char*)s;
    } while(*s++);
    return ret;
}

PUT_IN_VRAM char *strpbrk(const char *s1, const char *s2)
{
    while(*s1)
        if(strchr(s2, *s1++))
            return (char*)--s1;
    return 0;
}

PUT_IN_VRAM inline int to_upper(const char c)
{
	return (c >= 'a' && c <= 'z')?(c - ('a'-'A')):c;
}

PUT_IN_VRAM int strcasecmp(const char* one,const char* another )
{
    for ( ; to_upper(*one) == to_upper(*another); ++one, ++another )
        if ( *one == '\0' )
            return 0;
    return to_upper(*one) - to_upper(*another);
}

PUT_IN_VRAM int strcmp(const char* one,const char* another )
{
    for ( ; *one == *another; ++one, ++another )
        if ( *one == '\0' )
            return 0;
    return *one - *another;
}

PUT_IN_VRAM uint32_t get_entrys_first_cluster(dir_entry_t* dir_entry)
{
	uint32_t first_cluster = dir_entry->regular_entry.cluster_nr_bottom | (dir_entry->regular_entry.cluster_nr_top << 16);
	
	if(first_cluster == 0)
	{
		return vram_cd->sd_info.root_directory_cluster;
	}	
	return first_cluster;
}

PUT_IN_VRAM void store_long_name_part(uint8_t* buffer, dir_entry_t* cur_dir_entry, int position)
{	
	for(int i=0; i<5; i++)
		buffer[position + 0 + i] = (READ_U16_SAFE(&cur_dir_entry->long_name_entry.name_part_one[i]) & 0xFF00)?
			'_':READ_U16_SAFE(&cur_dir_entry->long_name_entry.name_part_one[i]);
	
	for(int i=0; i<6; i++)
		buffer[position + 5 + i] = (cur_dir_entry->long_name_entry.name_part_two[i] & 0xFF00)?
			'_':cur_dir_entry->long_name_entry.name_part_two[i];
	
	for(int i=0; i<2; i++)
		buffer[position + 11 + i] = (cur_dir_entry->long_name_entry.name_part_three[i] & 0xFF00)?
			'_':cur_dir_entry->long_name_entry.name_part_three[i];
}

PUT_IN_VRAM void find_dir_entry(uint32_t cur_dir_cluster, const char* given_name, dir_entry_t* result, SEARCH_TYPE type)
{	
	void* tmp_buf = (void*)vram_cd;
	uint32_t cur_dir_sector = get_sector_from_cluster(cur_dir_cluster);
	read_sd_sectors_safe(cur_dir_sector, vram_cd->sd_info.nr_sectors_per_cluster, tmp_buf + 512);
	bool last_entry = false;
	
	bool found_long_name = false;
	uint8_t name_buffer[256];
	for(int i = 0; i < 256; i++)
		name_buffer[i] = 0;
	
	while(true)
	{
		dir_entry_t* dir_entries = (dir_entry_t*)(tmp_buf + 512);
		for(int i = 0; i < vram_cd->sd_info.nr_sectors_per_cluster * 512 / 32; i++)
		{
			dir_entry_t* cur_dir_entry = &dir_entries[i];
			
			if((cur_dir_entry->attrib & DIR_ATTRIB_LONG_FILENAME) == DIR_ATTRIB_LONG_FILENAME)
			{
				if(type == LONG_NAME)
				{
					//construct name				
					int name_part_order = cur_dir_entry->long_name_entry.order & ~0x40;
					if(name_part_order >= 1 && name_part_order <= 20)
					{					
						store_long_name_part(name_buffer, cur_dir_entry, (name_part_order - 1) * 13);
						if(name_part_order == 1)
						{
							found_long_name = true;
						}
					}
				}
			}
			else if((cur_dir_entry->attrib & DIR_ATTRIB_VOLUME_ID) == DIR_ATTRIB_VOLUME_ID)
			{
				if(type == LONG_NAME)
				{
					//skip VOLUME_ID
					for(int j = 0; j < 128; j++)
					{
						*(uint16_t*)(name_buffer + j) = 0x2020;
					}
					name_buffer[255] = 0;
					continue;
				}
			}
			else if(cur_dir_entry->regular_entry.record_type == 0)
			{
				last_entry = true;
				break;
			}
			else if(cur_dir_entry->regular_entry.record_type == 0xE5)
			{
				//erased
			}
			else
			{
				if(type == SHORT_NAME)
				{
					if( cur_dir_entry->regular_entry.short_name[0] == given_name[0] &&
						cur_dir_entry->regular_entry.short_name[1] == given_name[1] &&
						cur_dir_entry->regular_entry.short_name[2] == given_name[2] &&
						cur_dir_entry->regular_entry.short_name[3] == given_name[3] &&
						cur_dir_entry->regular_entry.short_name[4] == given_name[4] &&
						cur_dir_entry->regular_entry.short_name[5] == given_name[5] &&
						cur_dir_entry->regular_entry.short_name[6] == given_name[6] &&
						cur_dir_entry->regular_entry.short_name[7] == given_name[7] &&
						cur_dir_entry->regular_entry.short_name[8] == given_name[8] &&
						cur_dir_entry->regular_entry.short_name[9] == given_name[9] &&
						cur_dir_entry->regular_entry.short_name[10] == given_name[10])
					{
						arm9_memcpy16((uint16_t*)result, (uint16_t*)cur_dir_entry, sizeof(dir_entry_t) / 2);
						return;
					}	
				}
				else if(type == LONG_NAME)
				{
					int len = 0;
					
					if(!found_long_name)
					{
						for(int j = 0; j < 8; j++)
						{
							name_buffer[j] = cur_dir_entry->regular_entry.short_name[j];
							if(name_buffer[j] != ' ')
								len = j + 1;
						}
						if(cur_dir_entry->regular_entry.short_name[8] != ' ')
						{
							name_buffer[len++] = '.';
							for(int j = 0; j < 3; j++)
							{
								if(cur_dir_entry->regular_entry.short_name[8 + j] == ' ')
									break;
								name_buffer[len++] = cur_dir_entry->regular_entry.short_name[8 + j];
							}
						}
						name_buffer[len++] = '\0';
					}
					
					name_buffer[255] = 0;
					uint8_t* long_name_ptr = &name_buffer[strlen((char*)name_buffer)-1];
					while(long_name_ptr != name_buffer && (*long_name_ptr == '.' || *long_name_ptr == ' '))
						long_name_ptr--;
					long_name_ptr[1] = '\0';
					
					if(strcasecmp(given_name, (char*)name_buffer) == 0)
					{
						arm9_memcpy16((uint16_t*)result, (uint16_t*)cur_dir_entry, sizeof(dir_entry_t) / 2);
						return;						
					}
					
					for(int j = 0; j < 256/2; j++)
					{
						*(uint16_t*)(name_buffer + j) = 0x2020;
					}
					name_buffer[255] = 0;
					
					found_long_name = false;
				}
			}
		}
		if(last_entry) break;
		//follow the chain
		uint32_t next = get_cluster_fat_value_simple(cur_dir_cluster);
		if(next >= 0x0FFFFFF8)
		{
			break;
		}
		cur_dir_cluster = next;
		read_sd_sectors_safe(get_sector_from_cluster(cur_dir_cluster), vram_cd->sd_info.nr_sectors_per_cluster, tmp_buf + 512);
	}
	
	//return entry with root_dir_cluster
	result->regular_entry.short_name[0] = 0x00;
	result->regular_entry.cluster_nr_bottom  = 0x0000;
	result->regular_entry.cluster_nr_top = 0x0000;
}

PUT_IN_VRAM void get_full_long_name(uint32_t cur_dir_cluster, const char* given_short_name, uint8_t* long_name) 
{
	void* tmp_buf = (void*)vram_cd;
	uint32_t cur_dir_sector = get_sector_from_cluster(cur_dir_cluster);
	read_sd_sectors_safe(cur_dir_sector, vram_cd->sd_info.nr_sectors_per_cluster, tmp_buf + 512);

	bool found_long_name = false;
	uint8_t name_buffer[256];
	for(int i = 0; i < 256; i++)
		name_buffer[i] = 0;
				
	while(true)
	{
		dir_entry_t* dir_entries = (dir_entry_t*)(tmp_buf + 512);
		for(int i = 0; i < vram_cd->sd_info.nr_sectors_per_cluster * 512 / 32; i++)
		{
			dir_entry_t* cur_dir_entry = &dir_entries[i];
			
			if(cur_dir_entry->regular_entry.record_type == 0xE5)
			{
				//erased
			}
			else if((cur_dir_entry->attrib & DIR_ATTRIB_LONG_FILENAME) == DIR_ATTRIB_LONG_FILENAME)
			{
				//construct name				
				int name_part_order = cur_dir_entry->long_name_entry.order & ~0x40;
				if(name_part_order >= 1 && name_part_order <= 20)
				{					
					store_long_name_part(name_buffer, cur_dir_entry, (name_part_order - 1) * 13);
					if(name_part_order == 1)
					{
						found_long_name = true;
					}
				}
			}
			else if(cur_dir_entry->attrib & (DIR_ATTRIB_VOLUME_ID))
			{
				//skip VOLUME_ID				
				for(int j = 0; j < 128; j++)
				{
					*(uint16_t*)(name_buffer + j) = 0x2020;
				}
				name_buffer[255] = 0;
			}
			else if(cur_dir_entry->regular_entry.record_type == 0)
			{
				*(uint16_t*)long_name = 0x0000;
				return;
			}
			else 
			{
				if( cur_dir_entry->regular_entry.short_name[0] == given_short_name[0] &&
					cur_dir_entry->regular_entry.short_name[1] == given_short_name[1] &&
					cur_dir_entry->regular_entry.short_name[2] == given_short_name[2] &&
					cur_dir_entry->regular_entry.short_name[3] == given_short_name[3] &&
					cur_dir_entry->regular_entry.short_name[4] == given_short_name[4] &&
					cur_dir_entry->regular_entry.short_name[5] == given_short_name[5] &&
					cur_dir_entry->regular_entry.short_name[6] == given_short_name[6] &&
					cur_dir_entry->regular_entry.short_name[7] == given_short_name[7] &&
					cur_dir_entry->regular_entry.short_name[8] == given_short_name[8] &&
					cur_dir_entry->regular_entry.short_name[9] == given_short_name[9] &&
					cur_dir_entry->regular_entry.short_name[10] == given_short_name[10])
				{
					if(!found_long_name)
					{
						int len = 0;						
						for(int j = 0; j < 8; j++)
						{
							name_buffer[j] = cur_dir_entry->regular_entry.short_name[j];
							if(name_buffer[j] != ' ')
								len = j + 1;
						}
						if(cur_dir_entry->regular_entry.short_name[8] != ' ')
						{
							name_buffer[len++] = '.';
							for(int j = 0; j < 3; j++)
							{
								if(cur_dir_entry->regular_entry.short_name[8 + j] == ' ')
									break;
								name_buffer[len++] = cur_dir_entry->regular_entry.short_name[8 + j];
							}
						}
						name_buffer[len] = '\0';
					}
					
					for(int j = 0; j < 256/2; j++)
					{						
						*(uint16_t*)(long_name + j) = *(uint16_t*)(name_buffer + j);
					}
					long_name[255] = 0;
					return;	
				}
				else
				{
					for(int j = 0; j < 256/2; j++)
					{
						*(uint16_t*)(name_buffer + j) = 0x2020;
					}
					name_buffer[255] = 0;
					found_long_name = false;
				}
			}
		}
		//follow the chain
		uint32_t next = get_cluster_fat_value_simple(cur_dir_cluster);
		if(next >= 0x0FFFFFF8)
		{
			*((vu32*)0x06202000) = 0x5453414C; //LAST
			while(1);//last
		}
		cur_dir_cluster = next;
		read_sd_sectors_safe(get_sector_from_cluster(cur_dir_cluster), vram_cd->sd_info.nr_sectors_per_cluster, tmp_buf + 512);
	}
}

PUT_IN_VRAM int gen_short_name(uint8_t* long_name, uint8_t* short_name, uint32_t cur_dir_cluster)
{
	uint8_t short_name_buff[11];
	uint8_t* long_name_ptr = long_name;
	uint8_t* short_name_ptr = short_name;
	uint8_t* tail_ptr;
	uint8_t char_to_insert;
	int lossy_convertion = 0;
	
	if(strlen((char*)long_name) > 256)
		return -1;
	
	if(strpbrk((char*)long_name, "\\/:*?\"<>|")!=NULL)
		return -1;
	
	for(int i=0; i<11; i++)
		short_name[i] = ' ';
		
	while(*long_name_ptr == '.')
	{
		long_name_ptr++;
		lossy_convertion = 1;
	}
	
	while(short_name_ptr < (short_name+8) && *long_name_ptr != '\0' && *long_name_ptr != '.')
	{
		if(*long_name_ptr == ' ')
		{			
			lossy_convertion = 1;
			long_name_ptr++;
			continue;
		}
		
		char_to_insert = to_upper(*long_name_ptr);
		
		if(char_to_insert != *long_name_ptr)
		{
			lossy_convertion = 1;
		}
			
		if ((char_to_insert >= 0x00 && char_to_insert < 0x20) ||
			(char_to_insert >= 0x7F && char_to_insert <= 0xFF) ||
			(strchr("\"*+,/:;<=>?\\[]| ", char_to_insert) != NULL))
		{
			char_to_insert = '_';
			lossy_convertion = 1;
		}
		
		*short_name_ptr++ = char_to_insert;
		long_name_ptr++;
	}
	
	if(*long_name_ptr != '.' && *long_name_ptr != '\0')
	{
		lossy_convertion = 1;
	}
	
	long_name_ptr = (uint8_t*)strrchr((char*)long_name, '.');
		
	if(long_name_ptr != NULL && long_name_ptr != (uint8_t*)strchr((char*)long_name, '.'))
	{
		lossy_convertion = 1;
	}
	
	if(long_name_ptr != NULL && long_name_ptr[1] != '\0')
	{
		short_name_ptr = short_name + 8;
		long_name_ptr++;
		for(int i = 0; i < 3 && *long_name_ptr != '\0'; i++)
		{			
			if(*long_name_ptr == ' ')
			{			
				lossy_convertion = 1;
				long_name_ptr++;
				continue;
			}
			
			char_to_insert = to_upper(*long_name_ptr);
			
			if ((char_to_insert >= 0x00 && char_to_insert < 0x20) ||
				(char_to_insert >= 0x7F && char_to_insert <= 0xFF) ||
				(strchr("\"*+,/:;<=>?\\[]| ", char_to_insert) != NULL))
			{
				char_to_insert = '_';
				lossy_convertion = 1;
			}
			
			*short_name_ptr++ = char_to_insert;
			long_name_ptr++;
		}
	}	
		
	if(lossy_convertion)
	{
		dir_entry_t file;
		
		for(int32_t i=1; i<999999; i++)
		{
			for(int j = 0; j< 11; j++)
				short_name_buff[j] = short_name[j];
			
			tail_ptr = short_name_buff + 7;
			for(int32_t j = i; j>0; )
			{
				*tail_ptr = '0' + (j%10);
				tail_ptr--;
				j /= 10;
			}
			*tail_ptr = '~';
			
			short_name_ptr = short_name_buff;
			while(short_name_ptr < (short_name_buff+8) && *short_name_ptr != ' ')
				short_name_ptr++;
			
			if(tail_ptr - short_name_ptr > 0)
			{				
				while(tail_ptr != (short_name_buff + 8))
				{
					*short_name_ptr = *tail_ptr;
					short_name_ptr++;
					tail_ptr++;
				}
				while(short_name_ptr != tail_ptr)
				{
					*short_name_ptr = ' ';
					short_name_ptr++;
				}
			}
			
			find_dir_entry(cur_dir_cluster, (char*)short_name_buff, &file, SHORT_NAME);
			
			if(file.regular_entry.short_name[0] == 0)
			{
				for(int j=0; j<11; j++)
					short_name[j] = short_name_buff[j];
				break;
			}
		}		
	}
	
	return lossy_convertion;
}

PUT_IN_VRAM static uint8_t get_checksum(const uint8_t* short_name)
{	
	uint8_t Sum = 0;
	
	for (int i=11; i!=0; i--)
		Sum = ((Sum & 1) ? 0x80 : 0) + (Sum >> 1) + *short_name++;
	
	return (Sum);
}

PUT_IN_VRAM int write_entries_to_sd(const uint8_t* long_name, const uint8_t* short_name, const int entries_count, uint8_t attribute, const uint32_t cur_dir_cluster)
{
#ifndef ARM7_DLDI
	dir_entry_t cur_entry;
	uint16_t lnf_buffer[13];
	bool pad = false;
	bool entries_inserted = false;
	
	void* tmp_buf = (void*)vram_cd;
	uint32_t cur_cluster = cur_dir_cluster;
	uint32_t cur_dir_sector = get_sector_from_cluster(cur_cluster);
	read_sd_sectors_safe(cur_dir_sector, vram_cd->sd_info.nr_sectors_per_cluster, tmp_buf + 512);

	int index = entries_count-1;
	
	while(true)
	{
		dir_entry_t* dir_entries = (dir_entry_t*)(tmp_buf + 512);
		for(int i = 0; i < vram_cd->sd_info.nr_sectors_per_cluster * 512 / 32; i++)
		{
			dir_entry_t* cur_dir_entry = &dir_entries[i];
					
			if(cur_dir_entry->regular_entry.record_type == 0)
			{				
				if(index >= 0)
				{
					if(index > 0)
					{
						cur_entry.long_name_entry.order = (index==entries_count-1) ? (0x40 | index) : index;
						cur_entry.long_name_entry.attrib = DIR_ATTRIB_LONG_FILENAME;
						cur_entry.long_name_entry.long_entry_type = 0x00;
						cur_entry.long_name_entry.checksum = get_checksum(short_name);
						cur_entry.long_name_entry.reserved = 0x0000;
						
						for(int j=0; j<13; j++)
						{
							lnf_buffer[j] = pad? 0xFFFF : (uint16_t)long_name[(index-1)*13 + j];
							if(long_name[(index-1)*13 + j] == '\0' && !pad)
								pad = true;
						}
						pad = false;
						
						for(int j=0; j<5; j++)
							cur_entry.long_name_entry.name_part_one[j] = lnf_buffer[j];
						
						for(int j=0; j<6; j++)
							cur_entry.long_name_entry.name_part_two[j] = lnf_buffer[j+5];
						
						for(int j=0; j<2; j++)
							cur_entry.long_name_entry.name_part_three[j] = lnf_buffer[j+5+6];
						
						entries_inserted = true;
					}
					else if(index==0)
					{
						for(int j=0; j<11; j++)
						{		
							cur_entry.regular_entry.short_name[j] = short_name[j];
						}
						
						cur_entry.regular_entry.attrib = attribute;
						cur_entry.regular_entry.reserved = 0x00;
						cur_entry.regular_entry.creation_time_tenths_of_seconds = 0x00; 
						cur_entry.regular_entry.creation_time = 0x0000; 			// 00:00:00
						cur_entry.regular_entry.creation_date = 0x0022;				// 02/Jan/1980
						cur_entry.regular_entry.last_access_date = 0x0022;			// 02/Jan/1980
						cur_entry.regular_entry.cluster_nr_top = 0x0000;
						cur_entry.regular_entry.last_modification_time = 0x0000; 	// 00:00:00
						cur_entry.regular_entry.last_modification_date = 0x0022;	// 02/Jan/1980
						cur_entry.regular_entry.cluster_nr_bottom = 0x0000;
						cur_entry.regular_entry.file_size = 0x00000000;
						
						entries_inserted = true;
					}
					
					arm9_memcpy16((uint16_t*)cur_dir_entry, (uint16_t*)&cur_entry, sizeof(dir_entry_t) / 2);
					
					index--;
				}
				else
				{					
					write_sd_sectors_safe(get_sector_from_cluster(cur_cluster), vram_cd->sd_info.nr_sectors_per_cluster, tmp_buf + 512);
					return 0;
				}
			}
		}
		
		if(entries_inserted)
		{
			write_sd_sectors_safe(get_sector_from_cluster(cur_cluster), vram_cd->sd_info.nr_sectors_per_cluster, tmp_buf + 512);
			entries_inserted = false;///?
		}
		//follow the chain
		uint32_t next = get_cluster_fat_value_simple(cur_cluster);
		if(next >= 0x0FFFFFF8)
		{
			// Function to allocate new cluster needed
			*((vu32*)0x06202000) = 0x4C434E4E; //Needs new cluster
			while(1);
		}
		cur_cluster = next;
		read_sd_sectors_safe(get_sector_from_cluster(cur_cluster), vram_cd->sd_info.nr_sectors_per_cluster, tmp_buf + 512);
	}
#endif
	return 0;
}