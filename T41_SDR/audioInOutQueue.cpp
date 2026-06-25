
#include "audioInOutQueue.h"

//#include "SDT.h"

#include "ethernetQueue.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

  // send queued data to stream
void AudioInputFromQueue::update() {
  audio_block_t *blockL, *blockR;

  if(enabled && queue->popBlocks(blockL, blockR)) {
    // we have blocks to stream
    transmit(blockL, 0);
    transmit(blockR, 1);
    release(blockL);
    release(blockR);
  }
}

// store input stream in queue
void AudioOutputToQueue::update() {
  audio_block_t* blockL = receiveReadOnly(0);
  audio_block_t* blockR = receiveReadOnly(1);

  if(!enabled || !blockL || !blockR || !queue->pushBlocks(blockL, blockR)) {
    releaseBlocks(blockL, blockR);
  }
}
