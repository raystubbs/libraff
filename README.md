# LibRaff
A simple parsing and manipulation library for
[RIFF](https://en.wikipedia.org/wiki/Resource_Interchange_File_Format)
based formats such as: WAV, AVI, etc.

## Build
Build is simple, but there's no install rule.  So will have to put
the produced library files where you want them:

    git clone https://github.com/raystubbs/libraff
    cd libraff
    make
    make test

Note that the test uses some type coercions that will fail for big
endian architectures; this doesn't mean the library doesn't work.
It's just the test cases.  The library should be portable across
architectures.

## Parsing
Raff tries to keep everything simple, so keep the user from having
to worry about the complexities of memory management; and to allow
for reuse of certain blocks were it otherwise might not be possible,
Raff allocates memory in a pool associated with a raff_File* instance.
This means that any allocations made will be released only after
the respective raff_File* has been 'closed'.

To load a RAFF file we just say:

    raff_File* file = raff_openFile( "path/to/file" );

This'll return a new file instance on success, or NULL on failure,
in which case the error code can be retrieved with `raff_errorNum()`
and an accompanying message with `raff_errorMsg()`.

Or we can load the file from a more abstract 'stream' as:

    raff_File* file = raff_openStream( someStream );

Where `someStream` is a pointer to a `raff_Stream` instance, which
provides the methods:

    typedef struct raff_Stream {
        int  (*next)( raff_Stream* stream );
        void (*close)( raff_Stream* stream );
    } raff_Stream;

These'll be by the library to read the next byte in the stream
and close (clean up after) the stream.  Raff will pass the
original stream pointer to these calls, so something like:

    typedef struct {
        raff_Stream s;
        FILE*       f;
    } FileStream;
    
    int
    nextCb( raff_Stream* s ) {
        FileStream* fs = (FileStream*)s;
        return fgetc( fs->f );
    }
    
    ...
    
    FileStream fs;
    fs.s.next = nextCb;
    
    raff_File* file = raff_openStream( (raff_Stream*)&fs );

Is the expected implementation pattern.

Once we have an open file we can get its associated chunk with:

    raff_Chunk* chunk = raff_fileAsChunk( file );

Chunks are the main orgainizational unit in RIFF formats.  Raff
provides three types of chunks: normal chunks, list chunks, and riff
chunks.  Normal chunks are just interpretted as chunks of data,
but both list and riff chunks are interpretted as lists of other
chunks; these are given chunk IDs of 'LIST' and 'RIFF' respectively,
but have sub-IDs to identify the specific _type_ of _usage_ of the
field.  Raff abstracts chunks a bit by representing the 'LIST' and 'RIFF'
in their type field instead of as an ID, leaving the ID attribute
to be used by the sub-ID.  This makes for easier usage in practice,
especially if we wanna search for a specific ID since with this we
can search for more relevent IDs than 'LIST'.

Chunks can be converted to `raff_Data*` or `raff_List*` for lower
level access; depending on their type.  If an attempt to convert
to the wrong type is made then `NULL` is returned and the error
number set to `riff_ERR_NOT_LIST` or `riff_ERR_IS_LIST`.  The
base chunk for a parsed file will always be a RIFF chunk, which
is just another type of list; so we can say:

    raff_List* list = raff_chunkAsList( chunk );

We can iterate over the chunks of a list with the `raff_start()` and
`raff_next()` which reset the internal list cursor and return the
next chunk in the list respectively.  The latter returns `NULL` at
the end of the list.  We can also search for a specific chunk ID
in the list with `raff_find()`:

    raff_start( list );
    raff_Chunk* iter;
    while( ( iter = raff_next( list ) ) ) {
        ...
    }
    
    raff_ID     waveId = raff_newID( "WAVE" );
    raff_Chunk* waveCk = raff_find( list, waveId );

The `raff_find()` function returns if no such ID could be cound in
the list.

Normal (non-list) chunks can be converted to `raff_Data*` with:

    raff_Data* data = raff_chunkAsData( someDataChunk );

And the contents of the data can be accessed with:

    size_t      datLen = raff_dataSize( data );
    char const* datBuf = raff_dataContent( data );

The ID of any chunk can be accessed with:

    raff_ID ckId = raff_getID( someChunk );

And when we're done with the file just cleanup with:

    raff_closeFile( someRafFile );

This'll free all memory pooled by the file, so once it's called
all chunks, datas, and lists associated with the file become
invalid.

## Manipulation
For RIFF manipulation we instroduce a few more functions.  The
`riff_prepend()` function adds a chunk to be beginning and
`riff_append()` adds a chunk to the end of a list... but
each chunk can only belong to one list at a time, so if the
given chunk already 'belongs' to another list, then an assertion
failure will occur.  To get around this we can 'copy' a chunk
with `raff_copyChunk()`.  This'll perform a shallow copy, keeping
the underlying data buffer; but since chunks are immutable this
isn't an issue.

    raff_prepend( someList, someChunk );
    raff_append( someList, otherChunk );
    
    raff_Chunk* copy = raff_copyChunk( someChunk );

We can also perform a deep copy, which'll copy the chunk and its
contents to another file; so the new file has jurisdiction over
the chunk copy.  We do this with:

    raff_Chunk* copy = raff_copy( newFile, someChunk );

We can also create new files, lists, and datas; to be converted to
chunks and manipulated at will:

    raff_File* file = raff_newFile();
    raff_Data* data = raff_newData( file, someId, buffer, size );
    raff_List* list = raff_newList( file, someId );

The new data will be created with a copy of the given buffer, and the
new list will be empty.  It's important to note that files created
in this way have no underlying `chunk`; so calls to `raff_fileAsChunk()`
will return NULL.  And IDs can be converted from string form with:

    raff_ID id = newID( "THIS" );

The max size for the given string is 4 bytes, shorter strings will
be padded with 0 in the serialized RIFF format.  And once all
manipluation is done we can write a chunk out to a buffer as:

    raff_serializeChunkToFile( someChunk, "path/to/file" );

We can also create an abstract stream of the serialized RIFF content
with:

    raff_Stream* stream = raff_serializeChunk( someChunk );

The returned stream has two methods `next()` and `close()`.  The
first should be called to get the next byte in the stream, and
terminates with `-1`.  The latter should be called once you're
finished with the stream to perform any cleanup.

    int nextByte = stream->next( stream );
    ...
    stream->close( stream );


