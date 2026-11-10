__attribute__((noinline)) ULONG CSystem::UpdateScanFrame(bool draw)
{
   gEndOfFrame = FALSE;
   gRenderFrame = draw;
   ULONG begin = gSystemCycleCount, last_service = begin;
#if R8_LYNX_HOTSET
   // Devices are owned for the entire frame; the platform service cannot replace them.
   C65C02 * const cpu=mCpu;
   CMikie * const mikie=mMikie;
#else
#define cpu mCpu
#define mikie mMikie
#endif
#if R8_LYNX_SHORT
   uint32_t short_hits=0;
#if R8_LYNX_SHORT_MISS > 0
   uint32_t probe_cooldown=0;
#endif
#endif

   while(gEndOfFrame != TRUE && !gSystemHalt && ((gSystemCycleCount-begin)&0x7fffffff)<1000000)
   {
      if(gSystemCycleCount>=gNextTimerEvent)
      {
#if R8_LYNX_COST
         lmc_update(mikie,8);
#else
         mikie->Update();
#endif
      }

#if R8_LYNX_SHORT
#if R8_LYNX_SHORT_MISS > 0
      if(probe_cooldown){--probe_cooldown;cpu->Update();}
      else if(cpu->UpdateShort())++short_hits;
      else probe_cooldown=R8_LYNX_SHORT_MISS;
#else
#if R8_LYNX_WAIT
      { ULONG batch=cpu->TryBootScan(last_service,begin);
        if(batch)short_hits+=batch;
        else short_hits+=cpu->UpdateShort(last_service,begin); }
#else
      if(cpu->UpdateShort())++short_hits;
#endif
#endif
#else
      cpu->Update();
#endif
      if(((gSystemCycleCount-last_service)&0x7fffffff)>=8192){n3_lynx_service();last_service=gSystemCycleCount;}

   #ifdef _LYNXDBG
            // Check breakpoint
            static ULONG lastcycle=0;
            if(lastcycle<mCycleCountBreakpoint && gSystemCycleCount>=mCycleCountBreakpoint) gBreakpointHit=TRUE;
            lastcycle=gSystemCycleCount;

            // Check single step mode
            if(gSingleStepMode) gBreakpointHit=TRUE;
   #endif

      if(gSystemCPUSleep)
      {
         gSystemCycleCount=gNextTimerEvent;
      }
   }
#if R8_LYNX_SHORT
   return short_hits;
#endif
}
