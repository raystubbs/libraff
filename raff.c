#include "raff.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct raff_Alloc {
    struct raff_Alloc* next;
    char data[];
} raff_Alloc;

typedef struct raff_File {
    raff_Alloc*  allocs;
    raff_Chunk*  chunk;
    size_t size;
    char   data[];
} raff_File;

typedef enum raff_Type {
    TYPE_LIST,
    TYPE_RIFF,
    TYPE_OTHER
} raff_Type;

typedef struct raff_Chunk {
    struct raff_Chunk* next;
    raff_File*         file;
    raff_List*         list;
    raff_Type          type;
    raff_ID            id;
    size_t             size;
    char*              start;
    
    raff_List*         asList;
    raff_Data*         asData;
} raff_Chunk;

typedef struct raff_List {
    raff_File*  file;
    raff_ID     id;
    raff_Chunk* cursor;
    raff_Chunk* first;
    raff_Chunk* last;
    
    raff_Chunk* asChunk;
} raff_List;

typedef struct raff_Data {
    raff_File*  file;
    raff_ID     id;
    size_t      size;
    char*       start;
    
    raff_Chunk* asChunk;
} raff_Data;

static raff_Error errnum = raff_ERR_NONE;

static int
snext( raff_Stream* stream ) {
    return stream->next( stream );
}

static void*
alloc( raff_File* file, size_t size ) {
    raff_Alloc* a = malloc( sizeof(raff_Alloc) + size );
    a->next = file->allocs;
    file->allocs = a;
    return a->data;
}

raff_ID*
parseID( raff_Stream* stream ) {

    long i1 = snext( stream );
    long i2 = snext( stream );
    long i3 = snext( stream );
    long i4 = snext( stream );
    if( i1 < 0 || i2 < 0 || i3 < 0 || i4 < 0 )
        return NULL;
    
    char idstr[5] = { i1, i2, i3, i4, 0 };
    
    static raff_ID id;
    id = raff_newID( idstr );
    return &id;
}

size_t*
parseSize( raff_Stream* stream ) {
    long s1 = snext( stream );
    long s2 = snext( stream );
    long s3 = snext( stream );
    long s4 = snext( stream );
    if( s1 < 0 || s2 < 0 || s3 < 0|| s4 < 0 )
        return NULL;
    
    static size_t size;
    size = s1 | s2 << 8 | s3 << 16 | s4 << 24;
    return &size;
}

static raff_ID RIFF_ID =
    (long)'R' << 24 | (long)'I' << 16 | (long)'F' << 8 | (long)'F';

static raff_ID LIST_ID =
    (long)'L' << 24 | (long)'I' << 16 | (long)'S' << 8 | (long)'T';

raff_File*
openStream( raff_Stream* stream ) {

    raff_ID* idp = parseID( stream );
    if( !idp || *idp != RIFF_ID ) {
        errnum = raff_ERR_NOT_RIFF;
        return NULL;
    }
    
    size_t* sizep = parseSize( stream );
    if( !sizep ) {
        errnum = raff_ERR_CORRUPT;
        return NULL;
    }
    size_t size = *sizep - 4;
    
    raff_ID* listIDp = parseID( stream );
    if( !listIDp ) {
        errnum = raff_ERR_CORRUPT;
        return NULL;
    }
    raff_ID listID = *listIDp;
    
    raff_File* file = malloc( sizeof(raff_File) + size );
    file->allocs = NULL;
    file->chunk  = NULL;
    file->size   = size;
    
    for( size_t i = 0 ; i < size ; i++ ) {
        int n = snext( stream );
        if( n < 0 ) {
            errnum = raff_ERR_CORRUPT;
            free( file );
            return NULL;
        }
        file->data[i] = n;
    }
    
    if( stream->close )
        stream->close( stream );
    
    raff_Chunk* chunk = alloc( file, sizeof(raff_Chunk) );
    chunk->next   = NULL;
    chunk->file   = file;
    chunk->list   = NULL;
    chunk->type   = TYPE_RIFF;
    chunk->id     = listID;
    chunk->size   = file->size;
    chunk->start  = file->data;
    chunk->asList = NULL;
    chunk->asData = NULL;
    file->chunk  = chunk;
    
    errnum = raff_ERR_NONE;
    return file;
}

raff_File*
raff_openStream( raff_Stream* stream ) {
    return openStream( stream );
}


typedef struct FileStream {
    raff_Stream stream;
    FILE*       file;
} FileStream;

static int
fnextCb( raff_Stream* stream ) {
    FileStream* fs = (FileStream*)stream;
    return fgetc( fs->file );
}

static void
fcloseCb( raff_Stream* stream ) {
    FileStream* fs = (FileStream*)stream;
    fclose( fs->file );
    free( stream );
}

raff_File*
raff_openFile( char const* path ) {
    FILE* file = fopen( path, "r" );
    if( !file ) {
        errnum = raff_ERR_CANT_OPEN;
        return NULL;
    }
    
    FileStream* stream = malloc( sizeof(FileStream) );
    stream->stream.next  = fnextCb;
    stream->stream.close = fcloseCb;
    stream->file = file;
    
    return openStream( (raff_Stream*)stream );
}

void
raff_closeFile( raff_File* file ) {
    while( file->allocs ) {
        raff_Alloc* a = file->allocs;
        file->allocs = file->allocs->next;
        
        free( a );
    }
    
    free( file );
}

raff_Error
raff_errorNum( void ) {
    return errnum;
}

char const*
raff_errorMsg( void ) {
    switch( errnum ) {
        case raff_ERR_NONE:
            return "Success";
        case raff_ERR_NOT_LIST:
            return "Requested list form from data chunk";
        case raff_ERR_IS_LIST:
            return "Requested data form from list chunk";
        case raff_ERR_NOT_RIFF:
            return "File does not begin with a RAFF chunk";
        case raff_ERR_CORRUPT:
            return "Invalid or corrup formatting";
        case raff_ERR_CANT_OPEN:
            return "Couldn't open file";
        default:
            return "You shouldn't get this";
    }
}

typedef struct ChunkStream {
    raff_Stream  stream;
    raff_Chunk*  chunk;
    size_t       next;
} ChunkStream;

static int
cnextCb( raff_Stream* stream ) {
    ChunkStream* cs = (ChunkStream*)stream;
    if( cs->next >= cs->chunk->size )
        return -1;
    
    return (unsigned char)cs->chunk->start[cs->next++];
}

static ChunkStream
makeChunkStream( raff_Chunk* chunk ) {
    ChunkStream stream;
    stream.stream.next  = cnextCb;
    stream.stream.close = NULL;
    stream.chunk = chunk;
    stream.next  = 0;
    
    return stream;
}

static raff_Chunk*
parseNextChunk( raff_File* file, ChunkStream* stream ) {
    raff_ID* idp = parseID( (raff_Stream*)stream );
    if( !idp ) {
        errnum = raff_ERR_CORRUPT;
        return NULL;
    }
    raff_ID id = *idp;
    
    size_t* sizep = parseSize( (raff_Stream*)stream );
    if( !sizep ) {
        errnum = raff_ERR_CORRUPT;
        return NULL;
    }
    size_t size = *sizep;
    
    // If size is odd then we need to skip the padding byte.
    bool pad = size % 2;
    
    raff_Chunk* chunk = alloc( file, sizeof(raff_Chunk) );
    chunk->next   = NULL;
    chunk->file   = file;
    chunk->list   = NULL;
    chunk->asList = NULL;
    chunk->asData = NULL;
    if( id == LIST_ID || id == RIFF_ID ) {
        raff_ID* listIDp = parseID( (raff_Stream*)stream );
        if( !listIDp ) {
            errnum = raff_ERR_CORRUPT;
            return NULL;
        }
        raff_ID listID = *listIDp;
        
        size -= 4;
        
        chunk->type = id == LIST_ID ? TYPE_LIST : TYPE_RIFF;
        chunk->id   = listID;
    }
    else {
        chunk->type = TYPE_OTHER;
        chunk->id   = id;
    }
    chunk->size  = size;
    chunk->start = stream->chunk->start + stream->next;
    
    stream->next += size + pad;
    if( stream->next > stream->chunk->size ) {
        errnum = raff_ERR_CORRUPT;
        return NULL;
    }
    
    errnum = raff_ERR_NONE;
    return chunk;
}

raff_Chunk*
raff_fileAsChunk( raff_File* file ) {
    return file->chunk;
}

raff_List*
raff_chunkAsList( raff_Chunk* chunk ) {
    if( chunk->type == TYPE_OTHER ) {
        errnum = raff_ERR_NOT_LIST;
        return NULL;
    }
    
    if( chunk->asList ) {
        errnum = raff_ERR_NONE;
        return chunk->asList;
    }
    
    raff_List* list = alloc( chunk->file, sizeof(raff_List) );
    list->file    = chunk->file;
    list->id      = chunk->id;
    list->asChunk = chunk;
    
    ChunkStream cs = makeChunkStream( chunk );
    
    raff_Chunk* firstChunk = NULL;
    raff_Chunk* lastChunk  = NULL;
    while( cs.next < chunk->size ) {
        
        raff_Chunk* sub = parseNextChunk( list->file, &cs );
        if( !sub )
            return NULL;
        
        sub->list = list;
        if( !firstChunk ) {
            firstChunk = sub;
            lastChunk  = sub;
        }
        else {
            lastChunk->next = sub;
            lastChunk = sub;
        }
    }
    
    list->cursor  = firstChunk;
    list->first   = firstChunk;
    list->last    = lastChunk;
    
    chunk->asList = list;
    errnum = raff_ERR_NONE;
    return list;
}

raff_Data*
raff_chunkAsData( raff_Chunk* chunk ) {
    if( chunk->type != TYPE_OTHER ) {
        errnum = raff_ERR_IS_LIST;
        return NULL;
    }
    if( chunk->asData ) {
        errnum = raff_ERR_NONE;
        return chunk->asData;
    }
    
    raff_Data* data = alloc( chunk->file, sizeof(raff_Data) );
    data->file    = chunk->file;
    data->id      = chunk->id;
    data->size    = chunk->size;
    data->start   = chunk->start;
    data->asChunk = chunk;
    
    chunk->asData = data;
    errnum = raff_ERR_NONE;
    return data;
}

static void
addID( char* data, size_t* next, raff_ID id ) {
    data[(*next)++] = id >> 24;
    data[(*next)++] = id >> 16;
    data[(*next)++] = id >> 8;
    data[(*next)++] = id;
}

static void
addSize( char* data, size_t* next, size_t size ) {
    data[(*next)++] = size;
    data[(*next)++] = size >> 8;
    data[(*next)++] = size >> 16;
    data[(*next)++] = size >> 24;
}

raff_Chunk*
raff_listAsChunk( raff_List* list, bool riff ) {
    if( list->asChunk && ( list->asChunk->type == TYPE_RIFF ) == riff ) {
        errnum = raff_ERR_NONE;
        return list->asChunk;
    }
    
    // Figure out list size.
    size_t      size = 0;
    raff_Chunk* iter = list->first;
    while( iter ) {
        
        // Size of chunk ID.
        size += 4;
        
        // Size of chunk size.
        size += 4;
        
        // If chunk is LIST or RIFF then size of sub ID.
        if( iter->type != TYPE_OTHER )
            size += 4;
        
        // Size of chunk data.
        size += iter->size;
        
        // If chunk data size is odd, then an extra padding byte.
        if( iter->size % 2 == 1 )
            size += 1;
        
        iter = iter->next;
    }
    
    // Allocate chunk.
    raff_Chunk* chunk = alloc( list->file, sizeof(raff_Chunk) );
    chunk->next   = NULL;
    chunk->file   = list->file;
    chunk->list   = NULL;
    chunk->type   = riff ? TYPE_RIFF : TYPE_LIST;
    chunk->id     = list->id;
    chunk->size   = size;
    chunk->start  = alloc( list->file, size );
    chunk->asList = list;
    chunk->asData = NULL;
    
    // Serialize list chunks.
    size_t i = 0;
    iter = list->first;
    while( iter ) {
        
        if( iter->type != TYPE_OTHER ) {
            // Add 'LIST' or 'RIFF' ID.
            if( iter->type == TYPE_RIFF )
                addID( chunk->start, &i, RIFF_ID );
            else
                addID( chunk->start, &i, LIST_ID );
            
            // Add chunk size.
            addSize( chunk->start, &i, iter->size );
            
            // Add sub-ID.
            addID( chunk->start, &i, iter->id );
        }
        else {
            // Add ID.
            addID( chunk->start, &i, iter->id );
            
            // Add chunk size.
            addSize( chunk->start, &i, iter->size );
        }
        
        // Add data.
        for( size_t j = 0 ; j < iter->size ; j++ )
            chunk->start[i++] = iter->start[j];
        
        // If chunk size is odd then add padding byte.
        if( iter->size % 2 == 1 )
            chunk->start[i++] = 0;
        
        iter = iter->next;
    }
    
    
    list->asChunk = chunk;
    
    errnum = raff_ERR_NONE;
    return chunk;
}

raff_Chunk*
raff_dataAsChunk( raff_Data* data ) {
    if( data->asChunk ) {
        errnum = raff_ERR_NONE;
        return data->asChunk;
    }
    
    raff_Chunk* chunk = alloc( data->file, sizeof(raff_Chunk) );
    chunk->next   = NULL;
    chunk->file   = data->file;
    chunk->list   = NULL;
    chunk->type   = TYPE_OTHER;
    chunk->id     = data->id;
    chunk->size   = data->size;
    chunk->start  = data->start;
    chunk->asData = data;
    
    data->asChunk = chunk;
    
    errnum = raff_ERR_NONE;
    return chunk;
}

void
raff_start( raff_List* list ) {
    list->cursor = list->first;
}

raff_Chunk*
raff_next( raff_List* list ) {
    raff_Chunk* next = list->cursor;
    if( list->cursor )
        list->cursor = list->cursor->next;
    return next;
}

void
raff_prepend( raff_List* list, raff_Chunk* chunk ) {
    // Since we're updating the list it'll no longer
    // reflect its ->asChunk field, so we clear it
    // and ->asChunk->asList first if this is set to
    // the list being modified.
    if( list->asChunk ) {
        if( list->asChunk->asList == list )
            list->asChunk->asList = NULL;
        list->asChunk = NULL;
    }
    
    // Chunk should be of same file as list.
    assert( chunk->file == list->file );
    
    // Chunk should not have a list.
    assert( chunk->list == NULL );
    
    chunk->next  = list->first;
    list->first = chunk;
    if( !list->last )
        list->last = list->first;
}

void
raff_append( raff_List* list, raff_Chunk* chunk ) {
    // Since we're updating the list it'll no longer
    // reflect its ->asChunk field, so we clear it
    // and ->asChunk->asList first if this is set to
    // the list being modified.
    if( list->asChunk ) {
        if( list->asChunk->asList == list )
            list->asChunk->asList = NULL;
        list->asChunk = NULL;
    }
    
    // Chunk should be of same file as list.
    assert( chunk->file == list->file );
    
    // Chunk should not have a list.
    assert( chunk->list == NULL );
    
    if( list->last )
        list->last->next = chunk;
    list->last = chunk;
    if( !list->first )
        list->first = list->last;
}

raff_File*
raff_newFile( void ) {
    raff_File* file = malloc( sizeof(raff_File) );
    file->allocs = NULL;
    file->chunk  = NULL;
    file->size   = 0;
    
    return file;
}

raff_Data*
raff_newData( raff_File* file, raff_ID id, char const* content, size_t size ) {
    raff_Data* data = alloc( file, sizeof(raff_Data) );
    data->file    = file;
    data->id      = id;
    data->size    = size;
    data->start   = alloc( file, size );
    data->asChunk = NULL;
    
    for( size_t i = 0 ; i < size ; i++ )
        data->start[i] = content[i];
    return data;
}

raff_List*
raff_newList( raff_File* file, raff_ID id ) {
    raff_List* list = alloc( file, sizeof(raff_List) );
    list->file    = file;
    list->id      = id;
    list->cursor  = NULL;
    list->first   = NULL;
    list->asChunk = false;
    
    return list;
}

raff_ID
raff_newID( char const* idstr ) {
    raff_ID id = 0;
    
    if( !idstr[0] )
        return id;
    id |= (long)idstr[0] << 24;
    
    if( !idstr[1] )
        return id;
    id |= (long)idstr[1] << 16;
    
    if( !idstr[2] )
        return id;
    id |= (long)idstr[2] << 8;
    
    if( !idstr[3] )
        return id;
    id |= idstr[3];
    
    return id;
}

raff_ID
raff_getID( raff_Chunk* chunk ) {
    return chunk->id;
}

raff_Chunk*
raff_findID( raff_List* list, raff_ID id ) {
    
    raff_Chunk* iter = list->first;
    while( iter ) {
        if( iter->id == id )
            return iter;
        
        iter = iter->next;
    }
    
    return NULL;
}


raff_Chunk*
raff_copyChunk( raff_Chunk* chunk ) {
    
    raff_Chunk* copy = alloc( chunk->file, sizeof(raff_Chunk) );
    copy->next   = NULL;
    copy->file   = chunk->file;
    copy->list   = NULL;
    copy->type   = chunk->type;
    copy->id     = chunk->id;
    copy->size   = chunk->size;
    copy->start  = chunk->start;
    copy->asList = NULL;
    copy->asData = NULL;
    
    return copy;
}


raff_Chunk*
raff_copyChunkTo( raff_File* file, raff_Chunk* chunk ) {
    
    raff_Chunk* copy = alloc( file, sizeof(raff_Chunk) );
    copy->next   = NULL;
    copy->file   = file;
    copy->list   = NULL;
    copy->type   = chunk->type;
    copy->id     = chunk->id;
    copy->size   = chunk->size;
    copy->start  = alloc( file, copy->size );
    copy->asList = NULL;
    copy->asData = NULL;
    
    for( size_t i = 0 ; i < copy->size ; i++ )
        copy->start[i] = chunk->start[i];
    
    return copy;
}

typedef struct SerializationStream {
    raff_Stream stream;
    raff_Chunk* chunk;
    size_t      next;
} SerializationStream;

static int
snextCb( raff_Stream* stream ) {
    SerializationStream* ss = (SerializationStream*)stream;
    
    size_t i = ss->next++;
    
    // Main ID.
    if( i < 4 ) {
        raff_ID id;
        if( ss->chunk->type == TYPE_OTHER )
            id = ss->chunk->id;
        else
        if( ss->chunk->type == TYPE_LIST )
            id = LIST_ID;
        else
            id = RIFF_ID;
        
        return (unsigned char)( id  >> 8*( 3 - i ) );
    } 
    
    // Size.  If chunk is LIST or RIFF then add 4 for sub-ID.
    if( i < 8 ) {
        size_t size = ss->chunk->size;
        if( ss->chunk->type != TYPE_OTHER )
            size += 4;
        
        return (unsigned char)( size >> 8*(i - 4) );
    }
    
    // If chunk is a LIST of RIFF then the next 4 bytes
    // are for the sub-ID.
    if( ss->chunk->type != TYPE_OTHER ) {
        if( i < 12 ) {
            raff_ID id = ss->chunk->id;
            return (unsigned char)( id >> 8*( 11 - i ) );
        }
        i -= 12;
    }
    else {
        i -= 8;
    }
    
    if( i >= ss->chunk->size )
        return -1;
    
    return (unsigned char)ss->chunk->start[i];
}

static void
scloseCb( raff_Stream* stream ) {
    free( stream );
}

raff_Stream*
raff_serializeChunk( raff_Chunk* chunk ) {
    
    SerializationStream* ss = malloc( sizeof(SerializationStream) );
    ss->stream.next  = snextCb;
    ss->stream.close = scloseCb;
    ss->chunk = chunk;
    ss->next  = 0;
    
    return (raff_Stream*)ss;
}

raff_Error
raff_serializeChunkToFile( raff_Chunk* chunk, char const* path ) {
    FILE* file = fopen( path, "w" );
    if( !file ) {
        errnum = raff_ERR_CANT_OPEN;
        return errnum;
    }
    
    raff_Stream* stream = raff_serializeChunk( chunk );
    int next;
    while( ( next = snext( stream ) ) >= 0 )
        fputc( next, file );
    
    stream->close( stream );
    fclose( file );
    
    errnum = raff_ERR_NONE;
    return errnum;
}

size_t
raff_dataSize( raff_Data* data ) {
    return data->size;
}

char const*
raff_dataContent( raff_Data* data ) {
    return data->start;
}
