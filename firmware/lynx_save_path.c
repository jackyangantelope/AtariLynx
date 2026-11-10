#if !R8_LYNX_FS
/* SPDX-License-Identifier: BSD-3-Clause
 * Main-thread owned FatFS only. Load-time preparation; no file API in guard.
 * Absolute read/load/scan paths stay unchanged. Never move a file or directory.
 */
#include "lynx_save_path.h"
#include <string.h>
#ifndef LCP_MODE
#define LCP_MODE 2
#endif
_Static_assert(LCP_MODE==1 || LCP_MODE==2,"Fixed path selector required");
_Static_assert(FF_FS_RPATH>=1 && FF_VOLUMES==1,"Single-volume relative paths required");
const volatile uint32_t lcp_selector=LCP_MODE;
uint32_t lcp_mode,lcp_reason;
static FATFS *prepared_fs;
static WORD mount_id;
static DWORD directory_cluster;
static size_t leaf_offset;
static char prepared_path[160];
void lcp_reset(void){
    lcp_mode=lcp_selector==2?2u:1u;lcp_reason=LCP_UNPREPARED;
    prepared_fs=0;prepared_path[0]=0;leaf_offset=0;
}
FRESULT lcp_prepare(FATFS *fs,const char *directory,const char *absolute){
    prepared_fs=0;prepared_path[0]=0;leaf_offset=0;lcp_reason=LCP_UNPREPARED;
    if(lcp_mode!=2){lcp_reason=LCP_OFF;return FR_OK;}
    const char *leaf=strrchr(absolute,'/');size_t dirlen=strlen(directory),len=strlen(absolute);
    if(!fs || !leaf || !leaf[1] || len>=sizeof prepared_path || absolute[0]!='/' ||
       (size_t)(leaf-absolute)!=dirlen || memcmp(directory,absolute,dirlen))return FR_INVALID_NAME;
    /* Relative exFAT paths also depend on its mutable directory-chain cache.
     * Keep the original absolute transaction until that path is qualified. */
    if(fs->fs_type!=2 && fs->fs_type!=3){lcp_reason=LCP_FILESYSTEM;return FR_OK;}
    FRESULT rc=f_chdir(directory);
    if(rc!=FR_OK)return rc;
    if(!fs->fs_type)return FR_NOT_READY;
    if(fs->fs_type!=2 && fs->fs_type!=3){lcp_reason=LCP_FILESYSTEM;return FR_OK;}
    prepared_fs=fs;mount_id=fs->id;directory_cluster=fs->cdir;
    memcpy(prepared_path,absolute,len+1);leaf_offset=(size_t)(leaf-absolute)+1;lcp_reason=LCP_RELATIVE;
    return FR_OK;
}
const char *lcp_save_name(FATFS *fs,const char *absolute){
    if(lcp_mode!=2)lcp_reason=LCP_OFF;
    else if(!prepared_fs){if(lcp_reason!=LCP_FILESYSTEM)lcp_reason=LCP_UNPREPARED;}
    else if(fs!=prepared_fs || !fs->fs_type || fs->id!=mount_id)lcp_reason=LCP_MOUNT_CHANGED;
    else if(fs->cdir!=directory_cluster)lcp_reason=LCP_DIRECTORY_CHANGED;
    else if(strcmp(absolute,prepared_path))lcp_reason=LCP_PATH_CHANGED;
    else{lcp_reason=LCP_RELATIVE;return absolute+leaf_offset;}
    return absolute;
}

#else
#include "lynx_save_fs.inc"
#endif
