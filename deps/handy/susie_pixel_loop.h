// RP2350 alteration (2026-10-27): keep decoder state in automatic locals for
// one destination line. Guest RAM stores may alias object members from the
// compiler's perspective; locals avoid repeated member reloads/stores.
// No callbacks occur in this loop. Every exit joins EndWhile and writes back
// the exact final state. RAM accesses, pixel/collision code and cycles stay in
// their original order. This is not a decoded-pixel cache or a read skip.
#if R8_LYNX_LINECACHE
{
   ULONG mLineType=this->mLineType;
   ULONG mLineShiftRegCount=this->mLineShiftRegCount;
   ULONG mLineShiftReg=this->mLineShiftReg;
   ULONG mLineRepeatCount=this->mLineRepeatCount;
   ULONG mLinePixel=this->mLinePixel;
   ULONG mLinePacketBitsLeft=this->mLinePacketBitsLeft;
   UUWORD mTMPADR=this->mTMPADR;
   UUWORD mHSIZACUM=this->mHSIZACUM;
#endif
#if R8_LYNX_RUN && LRN_SHADOW
   // LSR015: conservatively reject any overlap with this encoded source line
   // including the three-byte decoder's two-byte lookahead. No callbacks or
   // decoder reads occur inside an accepted already-decoded packed run.
   bool lrn_line=false;
   if(lrn_mode==2){
      ++lrn_lines;
      if(mSPRHSIZ.Word==0x100 && hsign==1 && (mSPRCOLL_Collide || mSPRSYS_NoCollide)){
         ULONG source=mSPRDLINE.Word,bytes=1+(mLinePacketBitsLeft>>3)+2;
         if(bytes<=257 && source+bytes<=65536 && mLineBaseAddress<=65536-80){
            if(mLineBaseAddress+80<=source || source+bytes<=mLineBaseAddress){lrn_line=true;++lrn_eligible;}
            else ++lrn_alias;
         }
      }
   }
#endif
   // Now render an individual destination line
   while(true)
   {
         ULONG tmp;

         if(!mLineRepeatCount)
         {
            // Normal sprites fetch their counts on a packet basis
            if(mLineType!=line_abs_literal)
            {
               MY_GET_BITS(tmp,1)
               if(tmp) mLineType=line_literal; else mLineType=line_packed;
            }

            // Pixel store is empty what should we do
            switch(mLineType)
            {
               case line_abs_literal:
                  // This means end of line for us
                  mLinePixel=LINE_END;
                  goto EndWhile;
               case line_literal:
                  MY_GET_BITS(mLineRepeatCount,4)
                  mLineRepeatCount++;
                  break;
               case line_packed:
                  //
                  // From reading in between the lines only a packed line with
                  // a zero size i.e 0b00000 as a header is allowable as a packet end
                  //
                  MY_GET_BITS(mLineRepeatCount,4)
                  if(!mLineRepeatCount)
                  {
                     mLinePixel=LINE_END;
                     mLineRepeatCount++;
                     goto EndWhile;
                  }
                  else
                  {
                     MY_GET_BITS(tmp,mSPRCTL0_PixelBits)
                     pixel=mPenIndex[tmp];
                  }
                  mLineRepeatCount++;
                  break;
               default:
                  pixel = 0;
                  goto LoopContinue;
            }

         }
      /*
         if(pixel==LINE_END)
         {
               printf("ERROR!\n");
               goto EndWhile;
         }
      */
            mLineRepeatCount--;

            switch(mLineType)
            {
               case line_abs_literal:
                  MY_GET_BITS(pixel,mSPRCTL0_PixelBits)
                  // Check the special case of a zero in the last pixel
                  if(!mLineRepeatCount && !pixel)
                  {
                     mLinePixel=LINE_END;
                     goto EndWhile;
                  }
                  else
                     pixel=mPenIndex[pixel];
                  break;
               case line_literal:
                  MY_GET_BITS(tmp,mSPRCTL0_PixelBits)
                  pixel=mPenIndex[tmp];
                  break;
               case line_packed:
                  break;
               default:
                  pixel=0;
                  goto LoopContinue;
            }

   LoopContinue:;
#if R8_LYNX_RUN && LRN_SHADOW
      if(lrn_line && mLineType==line_packed && mLineRepeatCount>=1 && mLineRepeatCount<=15 &&
         pixel>0 && pixel<14 && mHSIZACUM.Byte.High==0 && hoff>=0 && hoff<HANDY_SCREEN_WIDTH-1){
         unsigned n=mLineRepeatCount+1;
         if(n>(unsigned)(HANDY_SCREEN_WIDTH-hoff))n=HANDY_SCREEN_WIDTH-hoff;
         lrn_fill(mRamPointer+mLineBaseAddress+((unsigned)hoff>>1),hoff&1,pixel,n);
         mCycles+=n*2*SPR_RDWR_CYC;
         mLineRepeatCount-=n-1;hoff+=(int)n;pixel_width=1;
         onscreen=TRUE;everonscreen=TRUE;
         ++lrn_runs;lrn_pixels+=n;
         continue;
      }
#endif

      // This is allowed to update every pixel
      mHSIZACUM.Word+=mSPRHSIZ.Word;
      pixel_width=mHSIZACUM.Byte.High;
      mHSIZACUM.Byte.High=0;

      for(int hloop=0;hloop<pixel_width;hloop++)
      {
         // Draw if onscreen but break loop on transition to offscreen
         if(hoff>=0 && hoff<HANDY_SCREEN_WIDTH)
         {
            //ProcessPixel(hoff,pixel);
            PROCESS_PIXEL
            onscreen=TRUE;
            everonscreen=TRUE;
         }
         else
         {
            if(onscreen) break;
         }
         hoff += hsign;
      }
   }
mLinePixel = pixel;
EndWhile:;

#if R8_LYNX_LINECACHE
   this->mLineType=mLineType;
   this->mLineShiftRegCount=mLineShiftRegCount;
   this->mLineShiftReg=mLineShiftReg;
   this->mLineRepeatCount=mLineRepeatCount;
   this->mLinePixel=mLinePixel;
   this->mLinePacketBitsLeft=mLinePacketBitsLeft;
   this->mTMPADR=mTMPADR;
   this->mHSIZACUM=mHSIZACUM;
}
#endif
