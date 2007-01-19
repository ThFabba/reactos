/*
 * PE Fixup Utility
 * Copyright (C) 2005 Filip Navara
 *
 * The purpose of this utility is fix PE binaries generated by binutils and
 * to manipulate flags that can't be set by binutils.
 *
 * Currently two features are implemented:
 *
 * - Setting flags on PE sections for use by drivers. The sections
 *   .text, .data, .idata, .bss are marked as non-pageable and
 *   non-discarable, section PAGE is marked as pageable and section
 *   INIT is marked as discaradable.
 *
 * - Sorting of export name table in executables. DLLTOOL has bug
 *   in sorting algorithm when the --kill-at flag is used. The exports
 *   are sorted in the decorated form and so the fastcall symbols are
 *   incorrectly put at the beginning of export table. This option
 *   allow to correct sort the table, so binary search can be used
 *   to process them.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

/* The following definitions are ripped from MinGW W32API headers. We don't
   use these headers directly in order to allow compilation on Linux hosts. */

typedef unsigned char BYTE, *PBYTE;
typedef unsigned short WORD;
typedef unsigned int DWORD;
typedef int LONG;

#define IMAGE_NUMBEROF_DIRECTORY_ENTRIES 16
#define IMAGE_SIZEOF_SHORT_NAME 8
#define IMAGE_DOS_SIGNATURE 0x5A4D
#define IMAGE_NT_SIGNATURE 0x00004550
#define IMAGE_SCN_MEM_DISCARDABLE 0x2000000
#define IMAGE_SCN_MEM_NOT_PAGED 0x8000000
#define FIELD_OFFSET(t,f) ((LONG)&(((t*)0)->f))
#define IMAGE_FIRST_SECTION(h) ((PIMAGE_SECTION_HEADER) ((unsigned long)h+FIELD_OFFSET(IMAGE_NT_HEADERS,OptionalHeader)+((PIMAGE_NT_HEADERS)(h))->FileHeader.SizeOfOptionalHeader))
#define IMAGE_DIRECTORY_ENTRY_EXPORT 0

#pragma pack(2)
typedef struct _IMAGE_DOS_HEADER {
	WORD e_magic;
	WORD e_cblp;
	WORD e_cp;
	WORD e_crlc;
	WORD e_cparhdr;
	WORD e_minalloc;
	WORD e_maxalloc;
	WORD e_ss;
	WORD e_sp;
	WORD e_csum;
	WORD e_ip;
	WORD e_cs;
	WORD e_lfarlc;
	WORD e_ovno;
	WORD e_res[4];
	WORD e_oemid;
	WORD e_oeminfo;
	WORD e_res2[10];
	LONG e_lfanew;
} IMAGE_DOS_HEADER,*PIMAGE_DOS_HEADER;
#pragma pack(4)
#pragma pack(4)
typedef struct _IMAGE_EXPORT_DIRECTORY {
	DWORD Characteristics;
	DWORD TimeDateStamp;
	WORD MajorVersion;
	WORD MinorVersion;
	DWORD Name;
	DWORD Base;
	DWORD NumberOfFunctions;
	DWORD NumberOfNames;
	DWORD AddressOfFunctions;
	DWORD AddressOfNames;
	DWORD AddressOfNameOrdinals;
} IMAGE_EXPORT_DIRECTORY,*PIMAGE_EXPORT_DIRECTORY;
typedef struct _IMAGE_FILE_HEADER {
	WORD Machine;
	WORD NumberOfSections;
	DWORD TimeDateStamp;
	DWORD PointerToSymbolTable;
	DWORD NumberOfSymbols;
	WORD SizeOfOptionalHeader;
	WORD Characteristics;
} IMAGE_FILE_HEADER, *PIMAGE_FILE_HEADER;
typedef struct _IMAGE_DATA_DIRECTORY {
	DWORD VirtualAddress;
	DWORD Size;
} IMAGE_DATA_DIRECTORY,*PIMAGE_DATA_DIRECTORY;
typedef struct _IMAGE_OPTIONAL_HEADER {
	WORD Magic;
	BYTE MajorLinkerVersion;
	BYTE MinorLinkerVersion;
	DWORD SizeOfCode;
	DWORD SizeOfInitializedData;
	DWORD SizeOfUninitializedData;
	DWORD AddressOfEntryPoint;
	DWORD BaseOfCode;
	DWORD BaseOfData;
	DWORD ImageBase;
	DWORD SectionAlignment;
	DWORD FileAlignment;
	WORD MajorOperatingSystemVersion;
	WORD MinorOperatingSystemVersion;
	WORD MajorImageVersion;
	WORD MinorImageVersion;
	WORD MajorSubsystemVersion;
	WORD MinorSubsystemVersion;
	DWORD Reserved1;
	DWORD SizeOfImage;
	DWORD SizeOfHeaders;
	DWORD CheckSum;
	WORD Subsystem;
	WORD DllCharacteristics;
	DWORD SizeOfStackReserve;
	DWORD SizeOfStackCommit;
	DWORD SizeOfHeapReserve;
	DWORD SizeOfHeapCommit;
	DWORD LoaderFlags;
	DWORD NumberOfRvaAndSizes;
	IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
} IMAGE_OPTIONAL_HEADER,*PIMAGE_OPTIONAL_HEADER;
typedef struct _IMAGE_NT_HEADERS {
	DWORD Signature;
	IMAGE_FILE_HEADER FileHeader;
	IMAGE_OPTIONAL_HEADER OptionalHeader;
} IMAGE_NT_HEADERS,*PIMAGE_NT_HEADERS;
typedef struct _IMAGE_SECTION_HEADER {
	BYTE Name[IMAGE_SIZEOF_SHORT_NAME];
	union {
		DWORD PhysicalAddress;
		DWORD VirtualSize;
	} Misc;
	DWORD VirtualAddress;
	DWORD SizeOfRawData;
	DWORD PointerToRawData;
	DWORD PointerToRelocations;
	DWORD PointerToLinenumbers;
	WORD NumberOfRelocations;
	WORD NumberOfLinenumbers;
	DWORD Characteristics;
} IMAGE_SECTION_HEADER,*PIMAGE_SECTION_HEADER;
#pragma pack(4)

/* End of ripped definitions */

typedef struct _export_t {
   DWORD name;
   WORD ordinal;
} export_t;

unsigned char *buffer;
PIMAGE_DOS_HEADER dos_header;
PIMAGE_NT_HEADERS nt_header;

static inline WORD dtohs(WORD in)
{
    PBYTE in_ptr = (PBYTE)&in;
    return in_ptr[0] | (in_ptr[1] << 8);
}

static inline WORD htods(WORD in)
{
    WORD out;
    PBYTE out_ptr = (PBYTE)&out;
    out_ptr[0] = in; out_ptr[1] = in >> 8;
    return out;
}

static inline DWORD dtohl(DWORD in)
{
    PBYTE in_ptr = (PBYTE)&in;
    return in_ptr[0] | (in_ptr[1] << 8) | (in_ptr[2] << 16) | (in_ptr[3] << 24);
}

static inline DWORD htodl(DWORD in)
{
    DWORD out;
    PBYTE out_ptr = (PBYTE)&out;
    out_ptr[0] = in      ; out_ptr[1] = in >> 8;
    out_ptr[2] = in >> 16; out_ptr[3] = in >> 24;
    return out;
}

void *rva_to_ptr(DWORD rva)
{
   PIMAGE_SECTION_HEADER section_header;
   unsigned int i;

   for (i = 0, section_header = IMAGE_FIRST_SECTION(nt_header);
        i < dtohl(nt_header->OptionalHeader.NumberOfRvaAndSizes);
        i++, section_header++)
   {
      if (rva >= dtohl(section_header->VirtualAddress) &&
          rva < dtohl(section_header->VirtualAddress) +
                dtohl(section_header->Misc.VirtualSize))
      {
         return buffer + rva - dtohl(section_header->VirtualAddress) +
                dtohl(section_header->PointerToRawData);
      }
   }

   return NULL;
}

int export_compare_func(const void *a, const void *b)
{
   const export_t *ap = a;
   const export_t *bp = b;
   char *an = rva_to_ptr(ap->name);
   char *bn = rva_to_ptr(bp->name);
   return strcmp(an, bn);
}

int main(int argc, char **argv)
{
   int fd_in, fd_out;
   long len;
   PIMAGE_SECTION_HEADER section_header;
   PIMAGE_DATA_DIRECTORY data_dir;
   unsigned int i;
   unsigned long checksum;
   int fixup_exports = 0;
   int fixup_sections = 0;

   /*
    * Process parameters.
    */

   if (argc < 2)
   {
      printf("Usage: %s <filename> <options>\n"
             "Options:\n"
             " -sections Sets section flags for PE image.\n"
             " -exports Sort the names in export table.\n",
             argv[0]);
      return 1;
   }

   for (i = 2; i < argc; i++)
   {
      if (!strcmp(argv[i], "-sections"))
         fixup_sections = 1;
      else if (!strcmp(argv[i], "-exports"))
         fixup_exports = 1;
      else
         { printf("Invalid option: %s\n", argv[i]); return 1; }
   }

   if (fixup_sections == 0 && fixup_exports == 0)
   {
      printf("Nothing to do.\n");
      return 0;
   }

   /*
    * Read the whole file to memory.
    */

   fd_in = open(argv[1], O_RDONLY | O_BINARY);
   if (fd_in == 0)
   {
      printf("Can't open input file.\n");
      return 1;
   }

   len = lseek(fd_in, 0, SEEK_END);
   if (len < sizeof(IMAGE_DOS_HEADER))
   {
      close(fd_in);
      printf("'%s' isn't a PE image (too short)\n", argv[1]);
      return 1;
   }

   /* Lower down we overwrite the byte at len, so here, we need at least
    * one more byte than len.  We'll be guaranteed one or two now. */
   buffer = malloc((len + 2) & ~1);
   if (buffer == NULL)
   {
      close(fd_in);
      printf("Not enough memory available.\n");
      return 1;
   }

   /* Read the whole input file into a buffer */
   lseek(fd_in, 0, SEEK_SET);
   read(fd_in, buffer, len);
   /* Here is where the block end overwrite was */
   if (len & 1)
      buffer[len] = 0;

   close(fd_in);

   /*
    * Check the headers and save pointers to them.
    */

   dos_header = (PIMAGE_DOS_HEADER)buffer;
   nt_header = (PIMAGE_NT_HEADERS)(buffer + dtohl(dos_header->e_lfanew));
   
   if (dtohs(dos_header->e_magic) != IMAGE_DOS_SIGNATURE ||
       dtohl(nt_header->Signature) != IMAGE_NT_SIGNATURE)
   {
      printf("'%s' isn't a PE image (bad headers)\n", argv[1]);
      free(buffer);
      return 1;
   }

   if (fixup_exports)
   {
      /* Sort export directory */
      data_dir = &nt_header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
      if (dtohl(data_dir->Size) != 0)
      {
         PIMAGE_EXPORT_DIRECTORY export_directory;
         DWORD *name_ptr;
         WORD *ordinal_ptr;
         export_t *exports;

         export_directory = (PIMAGE_EXPORT_DIRECTORY)rva_to_ptr(dtohl(data_dir->VirtualAddress));
         if (export_directory != NULL)
         {
            exports = malloc(sizeof(export_t) * dtohl(export_directory->NumberOfNames));
            if (exports == NULL)
            {
               printf("Not enough memory.\n");
               free(buffer);
               return 1;
            }

            name_ptr = (DWORD *)rva_to_ptr(dtohl(export_directory->AddressOfNames));
            ordinal_ptr = (WORD *)rva_to_ptr(dtohl(export_directory->AddressOfNameOrdinals));

            for (i = 0; i < dtohl(export_directory->NumberOfNames); i++)
            {
               exports[i].name = dtohl(name_ptr[i]);
               exports[i].ordinal = dtohl(ordinal_ptr[i]);
            }

            qsort(exports, dtohl(export_directory->NumberOfNames), sizeof(export_t),
                  export_compare_func);

            for (i = 0; i < dtohl(export_directory->NumberOfNames); i++)
            {
               name_ptr[i] = htodl(exports[i].name);
               ordinal_ptr[i] = htodl(exports[i].ordinal);
            }

            free(exports);
         }
      }
   }

   if (fixup_sections)
   {
      /* Update section flags */
      for (i = 0, section_header = IMAGE_FIRST_SECTION(nt_header);
           i < dtohl(nt_header->OptionalHeader.NumberOfRvaAndSizes);
           i++, section_header++)
      {
         if (!strcmp((char*)section_header->Name, ".text") ||
             !strcmp((char*)section_header->Name, ".data") ||
             !strcmp((char*)section_header->Name, ".idata") ||
             !strcmp((char*)section_header->Name, ".bss"))
         {
            section_header->Characteristics |= htodl(IMAGE_SCN_MEM_NOT_PAGED);
            section_header->Characteristics &= htodl(~IMAGE_SCN_MEM_DISCARDABLE);
         }
         else if (!strcmp((char*)section_header->Name, "INIT"))
         {
            section_header->Characteristics |= htodl(IMAGE_SCN_MEM_DISCARDABLE);
         }
         else if (!strcmp((char*)section_header->Name, "PAGE"))
         {
            section_header->Characteristics |= htodl(IMAGE_SCN_MEM_NOT_PAGED);
         }
      }
   }

   /* Recalculate checksum */
   nt_header->OptionalHeader.CheckSum = 0;
   checksum = 0;
   for (i = 0; i < len; i += 2)
   {
      checksum += *(unsigned short *)(buffer + i);
      checksum = (checksum + (checksum >> 16)) & 0xffff;
   }
   checksum += len;
   nt_header->OptionalHeader.CheckSum = htods(checksum);

   /* Write the output file */
   fd_out = open(argv[1], O_WRONLY | O_BINARY);
   write(fd_out, buffer, len);
   close(fd_out);

   return 0;
}
