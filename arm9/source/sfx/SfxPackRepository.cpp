#include "common.h"
#include "SfxPackRepository.h"
#include "SfxAssets.h"
#include "fat/ff.h"
#include <stdlib.h>
#include <string.h>
void SfxPackRepository::Scan(const bool* cancel) {
    _count=0; _truncated=_error=false;
    struct Job { DIR directory; FILINFO entry; };
    Job* job=static_cast<Job*>(calloc(1,sizeof(Job)));
    if (!job) { _error=true; return; }
    FRESULT r=f_opendir(&job->directory,SfxAssets::kBankRoot);
    if (r!=FR_OK) { _error=r!=FR_NO_FILE && r!=FR_NO_PATH; free(job); return; }
    unsigned visited=0;
    while (!(cancel && __atomic_load_n(cancel,__ATOMIC_ACQUIRE))) {
        r=f_readdir(&job->directory,&job->entry);
        if (r!=FR_OK) { _error=true; break; }
        if (!job->entry.fname[0]) break;
        if (++visited>4096) { _truncated=true; break; }
        if (!(job->entry.fattrib&AM_DIR) || !SfxAssets::SafeFolder(job->entry.fname) ||
            SfxAssets::IsDefault(job->entry.fname)) continue;
        if (_count==SfxPackLimits::kPacks) { _truncated=true; break; }
        // A directory defines a bank. Empty/partial/invalid-clip banks are still listed.
        strlcpy(_entries[_count++].folder,job->entry.fname,sizeof(_entries[0].folder));
    }
    f_closedir(&job->directory); free(job);
    for (unsigned i=1;i<_count;++i) {
        Entry value=_entries[i]; unsigned j=i;
        while (j && strcasecmp(_entries[j-1].folder,value.folder)>0) { _entries[j]=_entries[j-1]; --j; }
        _entries[j]=value;
    }
}
