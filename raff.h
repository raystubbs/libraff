#ifndef raff_h
#define raff_h
#include <stdbool.h>
#include <stddef.h>

typedef struct raff_Chunk raff_Chunk;
typedef struct raff_List  raff_List;
typedef struct raff_Data  raff_Data;
typedef struct raff_File  raff_File;
typedef long long raff_ID;

typedef enum raff_Error {
    raff_ERR_NONE,
    raff_ERR_NOT_LIST,
    raff_ERR_IS_LIST,
    raff_ERR_NOT_RIFF,
    raff_ERR_CORRUPT,
    raff_ERR_CANT_OPEN
} raff_Error;

typedef struct raff_Stream {
    int  (*next)( struct raff_Stream* stream );
    void (*close)( struct raff_Stream* stream );
} raff_Stream;

// Create an RIFF file representation from an arbitrary stream,
// returns NULL if the given stream doesn't have a valid RIFF
// header; and sets the error value to raff_ERR_NOT_RAFF.
raff_File*
raff_openStream( raff_Stream* stream );

// Open and parge a RIFF file, returns NULL if the given file
// doesn't have a RIFF header or can't be opened and sets the
// error value appropriately to raff_ERR_NOT_RAFF or
// raff_ERR_CANT_OPEN.
raff_File*
raff_openFile( char const* path );

// Close a RIFF file, releasing its resources.  All allocation
// functions are tied to a specific file, so releasing the file
// also releases these allocations.
void
raff_closeFile( raff_File* file );

// Return the last error code.
raff_Error
raff_errorNum( void );

// Return the error message associated with the last code.
char const*
raff_errorMsg( void );

// Return the file's root chunk.
raff_Chunk*
raff_fileAsChunk( raff_File* file );

// Parse a LIST chunk and return its contents as a list,
// if the current chunk's ID is not LIST then returns
// NULL and error value is set to raff_ERR_NOT_LIST.
raff_List*
raff_chunkAsList( raff_Chunk* chunk );

// Parse a data chunk and return its contents,
// if the current chunk is a list chunk then
// returns NULL and sets error value to raff_ERR_IS_LIST.
raff_Data*
raff_chunkAsData( raff_Chunk* chunk );

// Encode a list as a chunk.
raff_Chunk*
raff_listAsChunk( raff_List* list, bool riff );

// Encode a data as a chunk.
raff_Chunk*
raff_dataAsChunk( raff_Data* data );

// Sets the list's cursor to the start of the list.
void
raff_start( raff_List* list );

// Returns the next chunk of a list, or NULL if we've
// reached the final chunk.
raff_Chunk*
raff_next( raff_List* list );

// Adds a chunk to the beginning of a list.
void
raff_prepend( raff_List* list, raff_Chunk* chunk );

// Adds a chunk to the end of a list.
void
raff_append( raff_List* list, raff_Chunk* chunk );

// Creates a new empty raff file.  Files created this way
// have no chunk, so raff_fileAsChunk() returns NULL.
raff_File*
raff_newFile( void );

// Creates a new data with the given content.
raff_Data*
raff_newData( raff_File* file, raff_ID id, char const* data, size_t size );

// Creates a new empty list.
raff_List*
raff_newList( raff_File* file, raff_ID id );

// Creates an ID from the given 4 byte (max) string.
raff_ID
raff_newID( char const* id );

// Returns the ID of a raff_Chunk.
raff_ID
raff_getID( raff_Chunk* chunk );

// Returns the first instance of a chunk with the specified
// ID within the given list, or NULL if no such chunk exists.
raff_Chunk*
raff_findID( raff_List* list, raff_ID id );

// Copy a chunk, since chunks are immutable this is only
// really useful when the chunk already belongs to a list
// but must be added to another in the same file.
raff_Chunk*
raff_copyChunk( raff_Chunk* chunk );

// Copy a chunk to a different file.
raff_Chunk*
raff_copyChunkTo( raff_File* file, raff_Chunk* chunk );


// Serializes the specified chunk as a raff_Stream*
// which should be closed, but not freed, after use.
raff_Stream*
raff_serializeChunk( raff_Chunk* chunk );

// Serialize the specified chunk to the given file.  Returns
// 0 = raff_ERR_NONE on success or raff_ERR_CANT_OPEN if the
// file can't be opened.  The returned code will also be put
// in errnum to be retrieved by raff_errorNum().
raff_Error
raff_serializeChunkToFile( raff_Chunk* chunk, char const* path );

// Returns the size of a raff_Data.
size_t
raff_dataSize( raff_Data* data );

// Returns the content of a raff_Data.
char const*
raff_dataContent( raff_Data* data );

#endif
