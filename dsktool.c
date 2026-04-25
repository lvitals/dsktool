#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <stdint.h>
#include <fnmatch.h>

typedef unsigned char byte;
typedef unsigned short word;

typedef enum {
    DSK_180KB  = 184320,   // 40 tracks, 1 side, 9 sectors (SSDD)
    DSK_360KB  = 368640,   // 40 tracks, 2 sides, 9 sectors (DSDD)
    DSK_720KB  = 737280,   // 80 tracks, 2 sides, 9 sectors (DSDD)
    DSK_1440KB = 1474560   // 80 tracks, 2 sides, 18 sectors (DSHD)
} DiskType;


typedef struct {
    char name[9];
    char ext[4];
    int size;
    int hour, min, sec;
    int day, month, year;
    int first;
    int pos;
    int attr;
} fileinfo;

byte *dskimage;
byte *fat;
byte *direc;
byte *cluster;
int sectorsperfat, numberoffats, reservedsectors;
int bytespersector, direlements, fatelements;
int sectorspercluster, clusterbytes;
int availsectors;
DiskType disktype;
char *hidden_system_files[128];
int hidden_system_file_count;
char boot_label[12] = "NO NAME    ";

static const byte nextor_dos220_boot_code[] = {
    0xD0, 0xED, 0x53, 0x7B, 0xC0, 0x11, 0x78, 0xC0,
    0x73, 0x23, 0x72, 0x11, 0x80, 0xC0, 0x0E, 0x0F,
    0xCD, 0x7D, 0xF3, 0x3C, 0xCA, 0x22, 0x40, 0x11,
    0x00, 0x01, 0x0E, 0x1A, 0xCD, 0x7D, 0xF3, 0x21,
    0x01, 0x00, 0x22, 0x8E, 0xC0, 0x21, 0x00, 0x3F,
    0x11, 0x80, 0xC0, 0x0E, 0x27, 0xD5, 0xCD, 0x7D,
    0xF3, 0xD1, 0x0E, 0x10, 0xCD, 0x7D, 0xF3, 0xC3,
    0x00, 0x01, 0x68, 0xC0, 0xCD, 0x00, 0x00, 0xC3,
    0x22, 0x40, 0x00, 0x00, 0x4D, 0x53, 0x58, 0x44,
    0x4F, 0x53, 0x20, 0x20, 0x53, 0x59, 0x53, 0x00,
    0x00, 0x00, 0x00
};

// Helper function to detect disk type by size
DiskType detect_disk_type(size_t size) {
    switch(size) {
        case DSK_180KB:  return DSK_180KB;
        case DSK_360KB:  return DSK_360KB;
        case DSK_720KB:  return DSK_720KB;
        case DSK_1440KB: return DSK_1440KB;
        default:         return 0;
    }
}

static void initialize_boot_parameters(void) {
    bytespersector = 512;
    reservedsectors = 1;
    numberoffats = 2;

    switch(disktype) {
        case DSK_180KB:
            sectorspercluster = 1;
            direlements = 64;
            sectorsperfat = 2;
            break;
        case DSK_360KB:
            sectorspercluster = 2;
            direlements = 112;
            sectorsperfat = 2;
            break;
        case DSK_720KB:
            sectorspercluster = 2;
            direlements = 112;
            sectorsperfat = 3;
            break;
        case DSK_1440KB:
            sectorspercluster = 1;
            direlements = 224;
            sectorsperfat = 9;
            break;
        default:
            sectorspercluster = 2;
            direlements = 112;
            sectorsperfat = 3;
            break;
    }

    clusterbytes = bytespersector * sectorspercluster;
}

static void store_word_le(byte *target, word value) {
    target[0] = value & 0xFF;
    target[1] = value >> 8;
}

static void store_dword_le(byte *target, uint32_t value) {
    target[0] = value & 0xFF;
    target[1] = (value >> 8) & 0xFF;
    target[2] = (value >> 16) & 0xFF;
    target[3] = (value >> 24) & 0xFF;
}

static uint32_t create_serial_number(void) {
    uint32_t serial = (uint32_t)time(NULL);
    serial ^= (uint32_t)getpid() << 16;
    serial ^= (uint32_t)clock();
    return serial;
}

static void set_boot_label(const char *label) {
    size_t i;

    memset(boot_label, ' ', 11);
    boot_label[11] = '\0';
    for (i = 0; i < 11 && label[i]; i++) {
        boot_label[i] = label[i];
    }
}

static void initialize_boot_sector(void) {
    word total_sectors;
    word sectors_per_track;
    word heads;
    byte media_descriptor;

    initialize_boot_parameters();

    memset(dskimage, 0, 512);

    dskimage[0x00] = 0xEB;
    dskimage[0x01] = 0xFE;
    dskimage[0x02] = 0x90;
    memcpy(dskimage + 3, "NEXTOR20", 8);

    switch(disktype) {
        case DSK_180KB:
            total_sectors = 360;
            sectors_per_track = 9;
            heads = 1;
            media_descriptor = 0xFC;
            break;
        case DSK_360KB:
            total_sectors = 720;
            sectors_per_track = 9;
            heads = 2;
            media_descriptor = 0xFD;
            break;
        case DSK_720KB:
            total_sectors = 1440;
            sectors_per_track = 9;
            heads = 2;
            media_descriptor = 0xF9;
            break;
        case DSK_1440KB:
            total_sectors = 2880;
            sectors_per_track = 18;
            heads = 2;
            media_descriptor = 0xF0;
            break;
        default:
            total_sectors = 1440;
            sectors_per_track = 9;
            heads = 2;
            media_descriptor = 0xF9;
            break;
    }

    store_word_le(dskimage + 0x0B, bytespersector);
    dskimage[0x0D] = sectorspercluster;
    store_word_le(dskimage + 0x0E, reservedsectors);
    dskimage[0x10] = numberoffats;
    store_word_le(dskimage + 0x11, direlements);
    store_word_le(dskimage + 0x13, total_sectors);
    dskimage[0x15] = media_descriptor;
    store_word_le(dskimage + 0x16, sectorsperfat);
    store_word_le(dskimage + 0x18, sectors_per_track);
    store_word_le(dskimage + 0x1A, heads);
    store_word_le(dskimage + 0x1C, 0);

    dskimage[0x1E] = 0x18;
    dskimage[0x1F] = 0x1E;
    memcpy(dskimage + 0x20, "VOL_ID", 6);
    dskimage[0x26] = 0x00;
    store_dword_le(dskimage + 0x27, create_serial_number());
    memcpy(dskimage + 0x2B, boot_label, 11);
    memcpy(dskimage + 0x36, "FAT12   ", 8);
    memcpy(dskimage + 0x3E, nextor_dos220_boot_code, sizeof(nextor_dos220_boot_code));
    store_word_le(dskimage + 0x1FE, 0xAA55);
}

static byte media_descriptor_for_disk(void) {
    switch(disktype) {
        case DSK_180KB:
            return 0xFC;
        case DSK_360KB:
            return 0xFD;
        case DSK_720KB:
            return 0xF9;
        case DSK_1440KB:
            return 0xF0;
        default:
            return 0xF9;
    }
}

static void uppercase_filename(const char *name, char *buffer, size_t buffer_size) {
    size_t i;

    snprintf(buffer, buffer_size, "%s", name);
    for (i = 0; buffer[i]; i++) {
        buffer[i] = toupper((unsigned char)buffer[i]);
    }
}

static void add_hidden_system_file(const char *name) {
    char normalized[13];
    size_t length;

    if (hidden_system_file_count >= (int)(sizeof(hidden_system_files) / sizeof(hidden_system_files[0]))) {
        printf("Too many --hidden-system-file options\n");
        exit(17);
    }

    uppercase_filename(name, normalized, sizeof(normalized));
    length = strlen(normalized) + 1;
    hidden_system_files[hidden_system_file_count] = malloc(length);
    if (!hidden_system_files[hidden_system_file_count]) {
        perror("Memory allocation failed");
        exit(1);
    }
    memcpy(hidden_system_files[hidden_system_file_count], normalized, length);
    hidden_system_file_count++;
}

static int should_mark_as_hidden_system_file(const char *name) {
    char uppername[13];
    int i;

    uppercase_filename(name, uppername, sizeof(uppername));
    for (i = 0; i < hidden_system_file_count; i++) {
        if (strcmp(uppername, hidden_system_files[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

/// <summary>
/// Load the specified DSK file into memory
/// </summary>
void load_dsk(char *name, int error) {
    struct stat st;
    int file;

    if (stat(name, &st) == 0) {
        disktype = detect_disk_type(st.st_size);
        if (!disktype) {
            printf("Unsupported disk size: %ld bytes\n", (long)st.st_size);
            printf("Supported formats:\n");
            printf(" 180KB: %d bytes (SS 40-track)\n", DSK_180KB);
            printf(" 360KB: %d bytes (DS 40-track)\n", DSK_360KB);
            printf(" 720KB: %d bytes (DS 80-track)\n", DSK_720KB);
            printf("1440KB: %d bytes (DSHD 80-track)\n", DSK_1440KB);
            exit(2);
        }
    } else if (error) {
        perror("Error accessing .DSK file");
        exit(2);
    } else {
        disktype = DSK_720KB; // Default to 720KB if creating new image
    }

    dskimage = (byte *)malloc(disktype);
    if (!dskimage) {
        perror("Memory allocation failed");
        exit(1);
    }

    file = open(name, O_RDONLY);
    if (file < 0) {
        if (error) {
            perror("Error opening .DSK file");
            exit(2);
        }
        memset(dskimage, 0, disktype);
        initialize_boot_sector();
    } else {
        if (read(file, dskimage, disktype) != disktype) {
            perror("Error reading .DSK file");
            close(file);
            exit(2);
        }
        close(file);
    }

    if (dskimage[510] != 0x55 || dskimage[511] != 0xAA) {
        initialize_boot_parameters();
    } else {
        // Read disk parameters from boot sector
        bytespersector = *(word *)(dskimage + 0x0B);
        sectorspercluster = *(dskimage + 0x0D);
        reservedsectors = *(word *)(dskimage + 0x0E);
        numberoffats = *(dskimage + 0x10);
        sectorsperfat = *(word *)(dskimage + 0x16);
        direlements = *(word *)(dskimage + 0x11);
        clusterbytes = bytespersector * sectorspercluster;
    }

    fat = dskimage + bytespersector * reservedsectors;
    direc = fat + bytespersector * (sectorsperfat * numberoffats);
    cluster = direc + direlements * 32;

    // Calculate available sectors based on disk type
    int tracks, sectors_per_track, sides;
    switch(disktype) {
        case DSK_180KB:
            tracks = 40;
            sectors_per_track = 9;
            sides = 1;
            break;
        case DSK_360KB:
            tracks = 40;
            sectors_per_track = 9;
            sides = 2;
            break;
        case DSK_720KB:
            tracks = 80;
            sectors_per_track = 9;
            sides = 2;
            break;
        case DSK_1440KB:
            tracks = 80;
            sectors_per_track = 18;
            sides = 2;
            break;
        default:
            tracks = 80;
            sectors_per_track = 9;
            sides = 2;
            break;
    }

    int total_sectors = tracks * sectors_per_track * sides;
    availsectors = total_sectors - reservedsectors - (sectorsperfat * numberoffats);
    availsectors -= direlements * 32 / bytespersector;
    fatelements = availsectors / sectorspercluster;

    if (file < 0) {
        // Initialize FAT for new disk
        fat[0] = media_descriptor_for_disk();
        fat[1] = 0xFF;
        fat[2] = 0xFF;
    }
}

/// <summary>
/// Go to the next directory entry
/// </summary>
int next_link(int link) {
    int pos;

    pos = (link >> 1) * 3;
    if (link & 1)
        return (((int)(fat[pos + 2])) << 4) + (fat[pos + 1] >> 4);
    else
        return (((int)(fat[pos + 1] & 0xF)) << 8) + fat[pos];
}

/// <summary>
/// Remove a directory entry
/// </summary>
int remove_link(int link) {
    int pos;
    int current;

    pos = (link >> 1) * 3;
    if (link & 1) {
        current = (((int)(fat[pos + 2])) << 4) + (fat[pos + 1] >> 4);
        fat[pos + 2] = 0;
        fat[pos + 1] &= 0xF;
        return current;
    } else {
        current = (((int)(fat[pos + 1] & 0xF)) << 8) + fat[pos];
        fat[pos] = 0;
        fat[pos + 1] &= 0xF0;
        return current;
    }
}

/// <summary>
/// Store the fat table entry for a specified link
/// </summary>
void store_fat(int link, int next) {
    int pos;

    pos = (link >> 1) * 3;
    if (link & 1) {
        fat[pos + 2] = next >> 4;
        fat[pos + 1] &= 0xF;
        fat[pos + 1] |= (next & 0xF) << 4;
    } else {
        fat[pos] = next & 0xFF;
        fat[pos + 1] &= 0xF0;
        fat[pos + 1] |= next >> 8;
    }
}

/// <summary>
/// Get the information for a specified directory entry
/// </summary>
fileinfo *getfileinfo(int pos) {
    fileinfo *file;
    byte *dir;
    int i;

    dir = direc + pos * 32;
    if (*dir < 0x20 || *dir >= 0x80) return NULL;

    file = (fileinfo *)malloc(sizeof(fileinfo));
    for (i = 0; i < 8; i++)
        file->name[i] = dir[i] == 0x20 ? 0 : dir[i];
    file->name[8] = 0;

    for (i = 0; i < 3; i++)
        file->ext[i] = dir[i + 8] == 0x20 ? 0 : dir[i + 8];
    file->ext[3] = 0;

    file->size = *(int *)(dir + 0x1C);

    i = *(word *)(dir + 0x16);
    file->sec = (i & 0x1F) << 1;
    file->min = (i >> 5) & 0x3F;
    file->hour = i >> 11;

    i = *(word *)(dir + 0x18);
    file->day = i & 0x1F;
    file->month = (i >> 5) & 0xF;
    file->year = 1980 + (i >> 9);

    file->first = *(word *)(dir + 0x1A);
    file->pos = pos;
    file->attr = *(dir + 0xB);

    return file;
}

/// <summary>
/// Calculate the available space on the DSK
/// </summary>
int bytes_free(void) {
    int i, avail = 0;

    for (i = 2; i < 2 + fatelements; i++)
        if (!next_link(i)) avail++;
    return avail * clusterbytes;
}

/// <summary>
/// List the directory of a DSK with optional filter
/// </summary>
void list_dsk(const char *filter) {
    int i;
    fileinfo *file;
    char name[20], date[30], time[30], size[30];
    int files_shown = 0;

    // Display volume name
    for (i = 0; i < 8; i++) {
        name[i] = dskimage[3 + i];
    }
    name[8] = 0;
    printf("Name of volume: %s\n\n", name);

    // List all files in directory
    for (i = 0; i < direlements; i++) {
        file = getfileinfo(i);
        if (file != NULL) {
            // Format filename (with extension if present)
            if (file->ext[0])
                snprintf(name, sizeof(name), "%s.%s", file->name, file->ext);
            else
                strncpy(name, file->name, sizeof(name));

            // Apply filter if provided (only show matching files)
            if (filter == NULL || fnmatch(filter, name, FNM_NOESCAPE) == 0) {
                 // Format file information
                snprintf(size, sizeof(size), "%7d", file->size);
                if (file->attr & 0x8) strncpy(size, "  <VOL>", sizeof(size));
                if (file->attr & 0x10) strncpy(size, "  <DIR>", sizeof(size));
                
                snprintf(date, sizeof(date), "%d/%02d/%d", file->day, file->month, file->year);
                snprintf(time, sizeof(time), "%d:%02d:%02d", file->hour, file->min, file->sec);
                
                printf("%-13s %s %10s %8s\n", name, size, date, time);
                files_shown++;
            }
            free(file);
        }
    }

    // Display summary information
    printf("\n%d files shown\n", files_shown);
    printf("%d bytes free\n", bytes_free());
}

/// <summary>
/// Find the directory entry matching the supplied filename
/// </summary>
int match(fileinfo *file, char *name) {
    char *p = file->name;
    int status = 0, i;

    for (i = 0; i < 8; i++) {
        if (!*name)
            break;
        if (*name == '*') {
            status = 1;
            name++;
            break;
        }
        if (*name == '.')
            break;
        if (toupper(*name++) != toupper(*p++))
            return 0;
    }
    if (!status && i < 8 && *p != 0)
        return 0;
    p = file->ext;
    if (!*name && !*p) return 1;
    if (*name++ != '.') return 0;
    for (i = 0; i < 3; i++) {
        if (*name == '*')
            return 1;
        if (toupper(*name++) != toupper(*p++))
            return 0;
    }
    return 1;
}

/// <summary>
/// Work through the directory tree
/// </summary>
void parse_tree(char *name, void (*action)(fileinfo *)) {
    int i;
    fileinfo *file;

    for (i = 0; i < direlements; i++) {
        if ((file = getfileinfo(i)) != NULL) {
            if (match(file, name))
                action(file);
            free(file);
        }
    }
}

/// <summary>
/// Search the directory for a specified file or a default wildcard search
/// </summary>
void parse_dsk(int argc, char **argv, void (*action)(fileinfo *)) {
    int i;

    if (argc == 3)
        parse_tree("*.*", action);
    else
        for (i = 3; i < argc; i++)
            parse_tree(argv[i], action);
}

/// <summary>
/// Extract a file from the DSK
/// </summary>
void extract(fileinfo *file) {
    byte *buffer, *p;
    int fileid;
    char name[20];
    int current;

    printf("extracting %s.%s\n", file->name, file->ext);
    buffer = (byte *)malloc((file->size + clusterbytes - 1) & (~(clusterbytes - 1)));
    memset(buffer, 0x1a, file->size);
    snprintf(name, sizeof(name), "%s.%s", file->name, file->ext);
    fileid = open(name, O_WRONLY | O_CREAT, 0644);
    current = file->first;
    p = buffer;
    do {
        memcpy(p, cluster + (current - 2) * clusterbytes, clusterbytes);
        p += clusterbytes;
        current = next_link(current);
    } while (current != 0xFFF);
    write(fileid, buffer, file->size);
    close(fileid);
    free(buffer);
}

/// <summary>
/// Wipe a DSK by clearing the directory
/// </summary>
void wipe(fileinfo *file) {
    int current;

    current = file->first;
    do {
        current = remove_link(current);
    } while (current != 0xFFF);
    direc[file->pos * 32] = 0xE5;
}

/// <summary>
/// Remove a file from the DSK
/// </summary>
void deleteFile(fileinfo *file) {
    printf("deleting %s.%s\n", file->name, file->ext);
    wipe(file);
}

/// <summary>
/// Write the in memory copy to the DSK file.
/// </summary>
void flush_dsk(char *name) {
    int file = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file < 0) {
        perror("Error opening disk image for writing");
        exit(10);
    }
    
    // Update FAT copies
    for (int i = 1; i < numberoffats; i++) {
        memcpy(fat + (i * sectorsperfat * bytespersector), 
               fat, 
               sectorsperfat * bytespersector);
    }
    
    if (write(file, dskimage, disktype) != disktype) {
        perror("Error writing disk image");
        close(file);
        exit(11);
    }
    close(file);
}

/// <summary>
/// Get the 1st free directory
/// </summary>
int get_free(void) {
    int i;

    for (i = 2; i < 2 + fatelements; i++)
        if (!next_link(i)) return i;
    printf("Internal error\n");
    exit(5);
}

/// <summary>
/// Get the next free sector
/// </summary>
int get_next_free(void) {
    int i, status = 0;

    for (i = 2; i < 2 + fatelements; i++)
        if (!next_link(i)) {
            if (status) {
                return i;
            } else {
                status = 1;
            }
        }
    printf("Internal error\n");
    exit(5);
}

/// <summary>
/// Add a single file to the DSK
/// </summary>
void add_single_file(char *name, char *pathname) {
    int i, total;
    int found = 0;
    fileinfo *file;
    int fileid;
    int size;
    int first;
    int current;
    int next;
    int pos;
    char *p;
    char fullname[250];
    struct stat st;

    snprintf(fullname, sizeof(fullname), "%s%s", pathname, name);
    fileid = open(fullname, O_RDONLY);
    if (fileid < 0) {
        perror("Error opening file");
        exit(7);
    }

    for (i = 0; i < direlements; i++) {
        if ((file = getfileinfo(i)) != NULL) {
            if (match(file, name)) {
                found = 1;
                wipe(file);
            }
            free(file);
        }
    }

    if (fstat(fileid, &st)) {
        perror("Error getting file size");
        exit(8);
    }
    size = st.st_size;

    if (size > bytes_free()) {
        printf("disk full\n");
        exit(4);
    }

    if (found)
        printf("updating %s\n", name);
    else
        printf("  adding %s\n", name);

    for (i = 0; i < direlements; i++)
        if (direc[i * 32] < 0x20 || direc[i * 32] >= 0x80)
            break;
    if (i == direlements) {
        printf("directory full\n");
        exit(6);
    }
    pos = i;

    byte *buffer = (byte *)malloc((size + clusterbytes - 1) & (~(clusterbytes - 1)));
    read(fileid, buffer, size);
    close(fileid);

    total = (size + clusterbytes - 1) / clusterbytes;
    current = first = get_free();
    byte *b1 = buffer;
    for (i = 0; i < total;) {
        memcpy(cluster + (current - 2) * clusterbytes, b1, clusterbytes);
        b1 += clusterbytes;
        if (++i == total)
            next = 0xFFF;
        else
            next = get_next_free();
        store_fat(current, next);
        current = next;
    }

    memset(direc + pos * 32, 0, 32);
    memset(direc + pos * 32, 0x20, 11);
    i = 0;
    for (p = name; *p; p++) {
        if (*p == '.') {
            i = 8;
            continue;
        }
        direc[pos * 32 + i++] = toupper(*p);
    }

    if (should_mark_as_hidden_system_file(name)) {
        direc[pos * 32 + 0x0B] = 0x06; // Hidden + System
    }

    *(word *)(direc + pos * 32 + 0x1A) = first;
    *(int *)(direc + pos * 32 + 0x1C) = size;
    
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    *(word *)(direc + pos * 32 + 0x16) =
        (tm->tm_sec >> 1) + (tm->tm_min << 5) + (tm->tm_hour << 11);
    *(word *)(direc + pos * 32 + 0x18) =
        (tm->tm_mday) + ((tm->tm_mon + 1) << 5) + ((tm->tm_year + 1900 - 1980) << 9);

    free(buffer);
}

/// <summary>
/// Add files specified by a wildcard to the DSK
/// </summary>
void add_files(char *name) {
    DIR *dir;
    struct dirent *entry;
    char *temp1, *temp2;
    char path[250];
    char dirname[200];
    
    // Extract directory path from name
    temp1 = NULL;
    temp2 = name;
    while ((temp2 = strstr(temp2, "/")) != NULL) {
        temp1 = temp2;
        temp2++;
    }
    
    if (temp1 != NULL) {
        strncpy(dirname, name, temp1 - name);
        dirname[temp1 - name] = '\0';
        snprintf(path, sizeof(path), "%s/", dirname);
    } else {
        strcpy(dirname, ".");
        strcpy(path, "");
    }

    // Get the filename pattern
    char *pattern = temp1 ? temp1 + 1 : name;
    
    if ((dir = opendir(dirname)) != NULL) {
        while ((entry = readdir(dir)) != NULL) {
            if (fnmatch(pattern, entry->d_name, 0) == 0) {
                add_single_file(entry->d_name, path);
            }
        }
        closedir(dir);
    } else {
        perror("Error opening directory");
        exit(9);
    }
}

/// <summary>
/// Add files from an argument list to the DSK
/// </summary>
void add_to_dsk(int argc, char **argv) {
    int i;

    for (i = 3; i < argc; i++) {
        if (strncmp(argv[i], "--boot-label=", 13) == 0) {
            set_boot_label(argv[i] + 13);
            continue;
        }
        if (strncmp(argv[i], "--hidden-system-file=", 21) == 0) {
            add_hidden_system_file(argv[i] + 21);
            continue;
        }
        add_files(argv[i]);
    }
}

/// <summary>
/// Extract the boot sector from the DSK image to a file
/// </summary>
void extract_boot_sector(char *dsk_filename, char *output_filename) {
    load_dsk(dsk_filename, 1);
    
    int file = open(output_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file < 0) {
        perror("Error opening output file for boot sector");
        exit(12);
    }
    
    // Write the first 512 bytes (boot sector)
    if (write(file, dskimage, 512) != 512) {
        perror("Error writing boot sector");
        close(file);
        exit(13);
    }
    
    close(file);
    printf("Boot sector extracted to %s\n", output_filename);
}

/// <summary>
/// Write BOOT.BIN to the boot sector of the DSK image
/// </summary>
void write_boot_sector(char *dsk_filename, char *boot_filename) {
    int boot_file, bytes_read;
    byte boot_sector[512];

    // Open the boot file
    boot_file = open(boot_filename, O_RDONLY);
    if (boot_file < 0) {
        perror("Error opening boot file");
        exit(15);
    }

    // Read exactly 512 bytes (boot sector size)
    bytes_read = read(boot_file, boot_sector, 512);
    close(boot_file);

    if (bytes_read != 512) {
        printf("Boot file must be exactly 512 bytes\n");
        exit(16);
    }

    // Load the DSK image
    load_dsk(dsk_filename, 1);

    // Overwrite the boot sector
    memcpy(dskimage, boot_sector, 512);

    // Save the modified DSK
    flush_dsk(dsk_filename);
    printf("Boot sector updated with %s\n", boot_filename);
}

/// <summary>
/// Delete (clear) the boot sector of the DSK image
/// </summary>
void delete_boot_sector(char *dsk_filename, int fill_with_zero) {
    load_dsk(dsk_filename, 1);

    if (!fill_with_zero) {
        // Option 1: Fill with zeros (invalidates boot)
        memset(dskimage, 0, 512);
        printf("Boot sector cleared (filled with zeros)\n");
    } else {
        // Option 2: Restore default MSX boot sector
        initialize_boot_sector();
        printf("Boot sector restored to default MSX bootloader\n");
    }

    flush_dsk(dsk_filename);
}

int main(int argc, char **argv) {
    printf("DSK Tool v1.3\n\n");

    if (argc < 3) {
        printf("Copyright (C) 1998 by Ricardo Bittencourt\n");
        printf("Updated 2010 by Tony Cruise\n");
        printf("Updated 2025 by Leandro V. Catarin\n\n");

        printf("Usage: dsktool command archive [files]\n\n");
        printf("commands:\n");
        printf("\t\tls\tlist contents of .DSK\n");
        printf("\t\tx\textract files from .DSK (supports wildcards)\n");
        printf("\t\ta\tadd files to .DSK\n");
        printf("\t\trm\tdelete files from .DSK\n");
        printf("\t\t\toptions for 'a': --boot-label=LABEL, --hidden-system-file=NAME\n");
        printf("\t\tsx\textract boot sector to a file\n");
        printf("\t\tsw\twrite boot sector from a file\n");
        printf("\t\ts\tinitialize boot sector (0=zeros, 1=default)\n");
        printf("\nexamples:\n");
        printf("\t\tdsktool ls TALKING.DSK\n");
        printf("\t\tdsktool x TALKING.DSK FUZZ*.*\n");
        printf("\t\tdsktool a TALKING.DSK MSXDOS.SYS COMMAND.COM\n");
        printf("\t\tdsktool rm TALKING.DSK *.BAS *.BIN\n");
        printf("\t\tdsktool sx TALKING.DSK BOOT.BIN\n");
        printf("\t\tdsktool sw TALKING.DSK NEWBOOT.BIN\n");
        printf("\t\tdsktool s TALKING.DSK 0  (fill boot sector with zeros)\n");
        printf("\t\tdsktool s TALKING.DSK 1  (restore default MSX boot)\n");
        exit(1);
    }
    
    // Convert command to lowercase for case-insensitive comparison
    char *command = argv[1];
    for (int i = 0; command[i]; i++) {
        command[i] = tolower(command[i]);
    }
    
    if (strcmp(command, "ls") == 0) {
        load_dsk(argv[2], 1);
        if (argc > 3) {
            list_dsk(argv[3]);
        } else {
            list_dsk(NULL);
        }
    }
    else if (strcmp(command, "x") == 0) {
        load_dsk(argv[2], 1);
        parse_dsk(argc, argv, extract);
    }
    else if (strcmp(command, "rm") == 0) {
        load_dsk(argv[2], 1);
        parse_dsk(argc, argv, deleteFile);
        flush_dsk(argv[2]);
    }
    else if (strcmp(command, "a") == 0) {
        load_dsk(argv[2], 0);
        add_to_dsk(argc, argv);
        flush_dsk(argv[2]);
    }
    else if (strcmp(command, "sx") == 0) {
        if (argc < 4) {
            printf("Output filename required for boot sector extraction\n");
            exit(14);
        }
        extract_boot_sector(argv[2], argv[3]);
    }
    else if (strcmp(command, "sw") == 0) {
        if (argc < 4) {
            printf("Boot filename required for writing\n");
            exit(17);
        }
        write_boot_sector(argv[2], argv[3]);
    }
    else if (strcmp(command, "s") == 0) {
        if (argc < 4) {
            printf("Specify 0 (zeros) or 1 (default bootloader)\n");
            exit(18);
        }
        delete_boot_sector(argv[2], atoi(argv[3]));
    }
    else {
        printf("Command not supported\n");
        exit(3);
    }
    
    return 0;
}
