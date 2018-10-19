#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "raff.h"

// Tests generation capabilities of raff.  Note that I use
// coercions from char pointers to int pointers freely,
// but this'll only work correcly for little endian
// machines since RIFF is a little endian based format.
// So this test should fail on big endian architectures.
// This test creates a valid WAV file and stores it
// in sample.wav, to be parsed by test-parse.c.

int
main( void ) {

    // Create a new file.
    raff_File* file = raff_newFile();
    
    // Create the 'data' chunk of the WAV file.
    uint16_t samples[2][2];
    
    // Sample 1
    samples[0][0] = 65516;
    samples[0][1] = 1;
    
    // Sample 2
    samples[1][0] = 65508;
    samples[1][1] = 65533;
    
    raff_ID    dataID  = raff_newID( "data" );
    raff_Data* dataDat =
        raff_newData( file, dataID, (char*)samples, sizeof(samples) );
    
    // Re-interpret as chunk.
    raff_Chunk* dataCk = raff_dataAsChunk( dataDat );
    
    
    // Now make the 'fmt ' chunk which holds formatting info.
    char fmtBuf[16];
    
    // AudioFormat: 1 = linear samples, other = some form of encryption
    *( (uint16_t*)fmtBuf ) = 1;
    
    // NumChannels: Number of audio channels.
    *( (uint16_t*)( fmtBuf + 2 ) ) = 2;
    
    // SampleRate: number of samples per... second?
    *( (uint32_t*)( fmtBuf + 4 ) ) = 22050;
    
    // ByteRate: SampleRate*SampleSize (including all channels)
    *( (uint32_t*)( fmtBuf + 8 ) ) = 22050*2*2;
    
    // BlockAlign: Alignment of samples
    *( (uint16_t*)( fmtBuf + 12 ) ) = 4;
    
    // Number of bits per sample per channel.
    *( (uint16_t*)( fmtBuf + 14 ) ) = 16;
    
    raff_ID    fmtID  = raff_newID( "fmt " );
    raff_Data* fmtDat =
        raff_newData( file, fmtID, fmtBuf, sizeof(fmtBuf) );
    
    // Re-interpret as chunk.
    raff_Chunk* fmtCk = raff_dataAsChunk( fmtDat );
    
    
    // Now make the WAVE list which will contain the previous two
    // chunks.
    raff_ID    waveID = raff_newID( "WAVE" );
    raff_List* waveLs = raff_newList( file, waveID );
    
    
    // Prepend the 'data' chunk.
    raff_prepend( waveLs, dataCk );
    
    // Prepend the 'fmt' chunk.
    raff_prepend( waveLs, fmtCk );
    
    // Re-interpret as a RIFF chunk.
    raff_Chunk* waveCk = raff_listAsChunk( waveLs, true );
    
    // And write out to file.
    raff_serializeChunkToFile( waveCk, "sample.wav" );
    
    raff_closeFile( file );
    printf( "Generated: sample.wav\n" );
    return 0;
}
