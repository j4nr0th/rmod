//
// Created by jan on 30.5.2023.
//

#include "rmod.h"

#ifndef _WIN32
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

void* map_file_to_memory(const char* filename, u64* p_out_size)
{
    RMOD_ENTER_FUNCTION;
    static long PG_SIZE = 0;
    void* ptr = NULL;
    if (!PG_SIZE)
    {
        PG_SIZE = sysconf(_SC_PAGESIZE);
        if (PG_SIZE < -1)
        {
            RMOD_ERROR("sysconf did not find page size, reason: %s", RMOD_ERRNO_MESSAGE);
            PG_SIZE = 0;
            goto end;
        }
    }
    const int fd = open(filename, O_RDONLY);
    if (fd < -1)
    {
        RMOD_ERROR("Could not open a file descriptor for file \"%s\", reason: %s", filename, RMOD_ERRNO_MESSAGE);
        goto end;
    }
    struct stat fd_stats;
    if (fstat(fd, &fd_stats) < 0)
    {
        RMOD_ERROR("Could not retrieve stats for fd of file \"%s\", reason: %s", filename, RMOD_ERRNO_MESSAGE);
        close(fd);
        goto end;
    }

    u64 size = fd_stats.st_size + 1;  //  That extra one for the sick null terminator
    //  Round size up to the closest larger multiple of page size
    u64 extra = size % PG_SIZE;
    if (extra)
    {
        size += (PG_SIZE - extra);
    }
    assert(size % PG_SIZE == 0);
    ptr = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (ptr == MAP_FAILED)
    {
        RMOD_ERROR("Failed mapping file \"%s\" to memory, reason: %s", filename, RMOD_ERRNO_MESSAGE);
        ptr = NULL;
        goto end;
    }
    *p_out_size = size;

end:
    RMOD_LEAVE_FUNCTION;
    return ptr;
}

void unmap_file(void* ptr, u64 size)
{
    RMOD_ENTER_FUNCTION;
    int r = munmap(ptr, size);
    if (r < 0)
    {
        RMOD_ERROR("Failed unmapping pointer %p from memory, reason: %s", ptr, RMOD_ERRNO_MESSAGE);
    }
    RMOD_LEAVE_FUNCTION;
}


#else

void* map_file_to_memory2(const char* filename, u64* p_out_size)
{
    RMOD_ENTER_FUNCTION;
    void* ptr = NULL;
    FILE* fd = fopen(filename, "r");
    if (!fd)
    {
        RMOD_ERROR_CRIT("Could not open file \"%s\", reason: %s", filename, RMOD_ERRNO_MESSAGE);
        goto end;
    }
    fseek(fd, 0, SEEK_END);
    size_t size = ftell(fd) + 1;
    ptr = jalloc(size);
    fseek(fd, 0, SEEK_SET);
    if (ptr)
    {
        size_t v = fread(ptr, 1, size, fd);
        if (ferror(fd))
        {
            RMOD_ERROR("Failed calling fread on file \"%s\", reason: %s", filename, RMOD_ERRNO_MESSAGE);
            fclose(fd);
            goto end;
        }
        *p_out_size = v;
    }
    fclose(fd);

end:
    RMOD_LEAVE_FUNCTION;
    return ptr;
}

void unmap_file(void* ptr, u64 size)
{
    RMOD_ENTER_FUNCTION;
    (void)size;
    jfree(ptr);
    RMOD_LEAVE_FUNCTION;
}
#endif
