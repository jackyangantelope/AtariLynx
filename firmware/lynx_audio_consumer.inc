/* LFE030: verbatim029 consumer body; now is time_us_64()*44100. */
#if R8_LYNX_UNDER
    if(lu_enabled()) {
        lu_snapshot before={now,audio_next,frames,
            {source_enqueued,source_played,source_zero,under,resync,gSystemCycleCount,
             (uint16_t)count,playing,buffering}};
        lu_service_begin(&before);
    }
#endif
    if(now>audio_next+1000000ull*2048){audio_next=now;resync++;}
#if R8_QUEUE_DIAG
    uint64_t due_start=audio_next;
    unsigned queue_before=count;
#endif
    if(buffering&&count>=1764){
        buffering=false;
#if R8_QUEUE_DIAG
        if(queue_diag.active)queue_diag.w.leave++;
#endif
    }
    while(audio_next<=now){
        int l=0,r=0;
        if(playing&&!buffering){
            if(count){l=samples[rd][0];r=samples[rd][1];rd=(rd+1)&4095;count--;source_played++;}
            else{
                under++;buffering=true;source_zero++;
#if R8_QUEUE_DIAG
                queue_diag.w.enter++;
                if(!queue_diag.w.first_under.valid)
                    r8_queue_capture(&queue_diag,false,time_us_64(),frames,count,r8_pc(),buffering);
#endif
            }
        }else if(playing)source_zero++;
        hstx_push_audio_sample(l,r);audio_next+=1000000;
    }
#if R8_LYNX_UNDER
    if(lu_enabled()) {
        lu_counts after={source_enqueued,source_played,source_zero,under,resync,gSystemCycleCount,
                         (uint16_t)count,playing,buffering};
        lu_service_end(&after);
    }
#endif
    unsigned qlevel=hstx_di_queue_get_level();
    if(qlevel<q_observed_min)q_observed_min=qlevel;
    if(qlevel>q_observed_max)q_observed_max=qlevel;
#if R8_QUEUE_DIAG
    r8_queue_service_end(&queue_diag,queue_before,count,(uint32_t)((audio_next-due_start)/1000000u));
#endif
