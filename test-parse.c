#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "raff.h"

// Tests parsing capabilities of raff.  Note that I use
// coercions from char pointers to int pointers freely,
// but this'll only work correcly for little endian
// machines since WAV is a little endian based format.
// So this test should fail on big endian architectures.
// This test parses the sample.wav WAV file.

int
main( void ) {

    // Open the file.
    raff_File* file = raff_openFile( "sample.wav" );
    assert( file );
    
    // Re-interpret as a chunk.
    raff_Chunk* riffCk = raff_fileAsChunk( file );
    assert( riffCk );
    
    // Chunk type should be WAVE.  The library doesn't
    // track the LIST or RIFF types as actual IDs,
    // instead encoding them serparately as the chunk
    // type.  So when a chunk ID is requested raff
    // will return the sub-ID if the chunk is a LIST
    // or RIFF chunk.
    raff_ID riffID = raff_getID( riffCk );
    assert( riffID == raff_newID( "WAVE" ) );
    
    // Re-interpret the chunk as a list.
    raff_List* riffLs = raff_chunkAsList( riffCk );
    assert( riffLs );
    
    // Find the 'fmt ' chunk, which identifies the
    // file's formatting info.
    raff_ID     fmtID = raff_newID( "fmt " );
    raff_Chunk* fmtCk = raff_findID( riffLs, fmtID );
    assert( fmtCk );
    
    // Re-interpret the format chunk as data.
    raff_Data* fmtDat = raff_chunkAsData( fmtCk );
    assert( fmtDat );
    
    size_t      fmtLen = raff_dataSize( fmtDat );
    char const* fmtBuf = raff_dataContent( fmtDat );
    assert( fmtLen >= 16 );
    
    // AudioFormat: 1 = linear samples, other = some form of compression
    uint16_t audioFormat = *(uint16_t*)fmtBuf;
    assert( audioFormat == 1 );
    
    // NumChannels: Number of audio channels.
    uint16_t numChannels = *(uint16_t*)( fmtBuf + 2);
    assert( numChannels == 2 );
    
    // SampleRate: Number of samples per... second?
    uint32_t sampleRate = *(uint32_t*)( fmtBuf + 4 );
    assert( sampleRate == 22050 );
    
    // ByteRate: SampleRate*SampleSize (including all channels)
    uint32_t byteRate = *(uint32_t*)( fmtBuf + 8 );
    assert( byteRate == 22050*2*2 );
    
    // BlockAlign: Alignment of samples
    uint16_t blockAlign = *(uint16_t*)( fmtBuf + 12 );
    assert( blockAlign == 4 );
    
    // Number of bits per sample per channel.
    uint16_t bitsPerSample = *(uint16_t*)( fmtBuf + 14 );
    assert( bitsPerSample == 16 );
    
    // At this point we've verifies the format subchunk,
    // now let's try a few samples.
    
    // Find the 'data' chunk, which contains the waveform samples.
    raff_ID     dataID = raff_newID( "data" );
    raff_Chunk* dataCk = raff_findID( riffLs, dataID );
    assert( dataID );
    
    // Re-interpret as data.
    raff_Data* dataDat = raff_chunkAsData( dataCk );
    assert( dataDat );
    
    char const* dataBuf = raff_dataContent( dataDat );
    
    // Sample 1
    uint16_t s1c1 = *(uint16_t*)dataBuf;
    uint16_t s1c2 = *(uint16_t*)( dataBuf + 2 );
    assert( s1c1 == 65516 );
    assert( s1c2 == 1 );
    
    // Sample 2
    uint16_t s2c1 = *(uint16_t*)( dataBuf + 4 );
    uint16_t s2c2 = *(uint16_t*)( dataBuf + 6 );
    assert( s2c1 == 65508 );
    assert( s2c2 == 65533 );
    
    raff_closeFile( file );
    printf( "Passed: Parse Test\n" );
    return 0;
}
