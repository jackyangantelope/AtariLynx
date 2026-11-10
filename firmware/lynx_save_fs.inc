/* SPDX-License-Identifier: BSD-3-Clause
 * LFX027: qualified FAT12/16/32 and exFAT directory identity. Main thread only.
 * The guard never invokes FatFS, allocates, or restores a directory snapshot.
 * In particular d_size belongs to FatFS and changes when a directory grows.
 */
#include "lynx_save_path.h"
#include <string.h>
#ifndef LFX_MODE
#define LFX_MODE 2
#endif
_Static_assert(LFX_MODE==1 || LFX_MODE==2,"Fixed filesystem selector required");
_Static_assert(LCP_MODE==2,"LCP026 ON baseline required");
_Static_assert(FF_FS_RPATH>=1 && FF_VOLUMES==1 && FF_FS_EXFAT,"Qualified single-volume configuration required");
const volatile uint32_t lcp_selector=LCP_MODE;
const volatile uint32_t lfx_selector=LFX_MODE;
uint32_t lcp_mode,lcp_reason,lfx_mode;
static FATFS *prepared_fs;
static WORD mount_id;
static BYTE prepared_type;
static DWORD directory_cluster;
static size_t leaf_offset;
static char prepared_path[160];
static UINT chain_depth;
static DWORD chain_cluster[FF_PATH_DEPTH+1],chain_offset[FF_PATH_DEPTH];

const char *lfx_fs_name(unsigned type){
    switch(type){case FS_FAT12:return "FAT12";case FS_FAT16:return "FAT16";
        case FS_FAT32:return "FAT32";case FS_EXFAT:return "exFAT";
        case 0:return "unmounted";default:return "unknown";}
}
unsigned lfx_depth(const FATFS *fs){return fs && fs->fs_type==FS_EXFAT?fs->xcwds.depth:0;}
static int qualified(unsigned type){
    return type==FS_FAT16 || type==FS_FAT32 || (lfx_mode==2 && (type==FS_FAT12 || type==FS_EXFAT));
}
static int chain_valid(const FATFS *fs){
    const FFXCWDS *chain=&fs->xcwds;
    if(chain->depth>FF_PATH_DEPTH || chain->tbl[0].d_scl!=0)return 0;
    if((chain->depth==0)!=(fs->cdir==0) || chain->tbl[chain->depth].d_scl!=fs->cdir)return 0;
    for(UINT i=1;i<=chain->depth;i++){
        if(chain->tbl[i].d_scl<2 || chain->tbl[i].d_scl>=fs->n_fatent)return 0;
        if(chain->tbl[i-1].nxt_ofs%32)return 0;
    }
    return 1;
}
void lcp_reset(void){
    lcp_mode=lcp_selector==2?2u:1u;lfx_mode=lfx_selector==2?2u:1u;
    lcp_reason=LCP_UNPREPARED;prepared_fs=0;prepared_path[0]=0;leaf_offset=0;
}
FRESULT lcp_prepare(FATFS *fs,const char *directory,const char *absolute){
    prepared_fs=0;prepared_path[0]=0;leaf_offset=0;lcp_reason=LCP_UNPREPARED;
    if(lcp_mode!=2){lcp_reason=LCP_OFF;return FR_OK;}
    if(!fs || !directory || !absolute)return FR_INVALID_NAME;
    const char *leaf=strrchr(absolute,'/');size_t dirlen=strlen(directory),len=strlen(absolute);
    if(!leaf || !leaf[1] || len>=sizeof prepared_path || absolute[0]!='/' ||
       (size_t)(leaf-absolute)!=dirlen || memcmp(directory,absolute,dirlen))return FR_INVALID_NAME;
    if(!qualified(fs->fs_type)){lcp_reason=LCP_FILESYSTEM;return FR_OK;}
    WORD before_id=fs->id;BYTE before_type=fs->fs_type;
    FRESULT rc=f_chdir(directory);
    if(rc!=FR_OK)return rc;
    if(fs->id!=before_id || fs->fs_type!=before_type){lcp_reason=LCP_MOUNT_CHANGED;return FR_NOT_READY;}
    if(fs->fs_type==FS_EXFAT){
        if(!chain_valid(fs)){lcp_reason=LCP_DIRECTORY_CHANGED;return FR_INT_ERR;}
        chain_depth=fs->xcwds.depth;
        for(UINT i=0;i<=chain_depth;i++)chain_cluster[i]=fs->xcwds.tbl[i].d_scl;
        for(UINT i=0;i<chain_depth;i++)chain_offset[i]=fs->xcwds.tbl[i].nxt_ofs;
    }
    prepared_fs=fs;mount_id=fs->id;prepared_type=fs->fs_type;directory_cluster=fs->cdir;
    memcpy(prepared_path,absolute,len+1);leaf_offset=(size_t)(leaf-absolute)+1;lcp_reason=LCP_RELATIVE;
    return FR_OK;
}
const char *lcp_save_name(FATFS *fs,const char *absolute){
    if(lcp_mode!=2){lcp_reason=LCP_OFF;return absolute;}
    if(!prepared_fs){if(lcp_reason!=LCP_FILESYSTEM)lcp_reason=LCP_UNPREPARED;return absolute;}
    if(fs!=prepared_fs || fs->fs_type!=prepared_type || fs->id!=mount_id){lcp_reason=LCP_MOUNT_CHANGED;return absolute;}
    lcp_reason=LCP_DIRECTORY_CHANGED;
    if(fs->cdir!=directory_cluster)return absolute;
    if(fs->fs_type==FS_EXFAT){
        if(!chain_valid(fs) || fs->xcwds.depth!=chain_depth)return absolute;
        for(UINT i=0;i<=chain_depth;i++)if(fs->xcwds.tbl[i].d_scl!=chain_cluster[i])return absolute;
        for(UINT i=0;i<chain_depth;i++)if(fs->xcwds.tbl[i].nxt_ofs!=chain_offset[i])return absolute;
    }
    if(strcmp(absolute,prepared_path)){lcp_reason=LCP_PATH_CHANGED;return absolute;}
    lcp_reason=LCP_RELATIVE;return absolute+leaf_offset;
}
