/*
 * 7z.h
 */

#if !defined(__7Z_H__)
#define __7Z_H__

typedef unsigned long long int UINT64;
typedef unsigned int UINT32;

typedef enum _i7z_err {
  UNEXPECTED_EOF = -1,
  NOT_7Z_FORMAT = -2,
  UNEXPECTED_PROPERTYID = -3,
  NO_ENOUGH_MEMORY = -4,
  PROPERTY_ERROR = -5,
  LZMA_DATA_ERROR = -6,
  LZMA_NO_ENOUGH_BUFFER = -7,
  UNKNOWN_CODEC = -8,
  UNKNOWN_ERROR = -255,
} I7z_err;

typedef int (*I7z_read_func)(void *, unsigned char *, unsigned int);
typedef int (*I7z_seek_func)(void *, int, int);
typedef unsigned int (*I7z_tell_func)(void *);
typedef void (*I7z_close_func)(void *);
typedef void (*I7z_destroy_func)(void *);
enum { _I7z_SET = 0, _I7z_CUR = 1, _I7z_END = 2 };
typedef struct _i7z_stream {
  I7z_read_func read_func;
  I7z_seek_func seek_func;
  I7z_tell_func tell_func;
  I7z_close_func close_func;
  I7z_destroy_func destroy_func;
  void *obj;
} I7z_stream;

typedef struct _i7z_coder {
  int idsize; // real_idsize = idsize & 0xf
  int *ids;
  UINT64 ninstreams;
  UINT64 noutstreams;
  UINT64 propertiessize;
  unsigned char *properties;
  void *(*create)(I7z_stream *, unsigned char *, unsigned int);
  int (*decode)(void *, unsigned char *, UINT64 *);
  void (*destroy)(void *);
} I7z_coder;

typedef struct _i7z_folder {
  UINT64 ncoders;
  UINT64 ninstreams;
  UINT64 noutstreams;
  UINT64 *unpacksizes; //[0 .. noutstreams - 1]
  I7z_coder *coders;
  UINT32 *crcs;
  UINT64 nunsubstreams;
  UINT64 *unsubstreamsizes;
  UINT64 idx_base;
  UINT64 stream_base;
  UINT64 *inindexes;
  UINT64 *outindexes;
} I7z_folder;

typedef struct _i7z_pack {
  UINT64 packpos;
  UINT64 npackstreams;
  UINT64 *packsizes;
  //UINT64 *packstreamdigests;
  UINT64 nfolders;
  I7z_folder *folders;
  UINT32 *subcrcs;
} I7z_pack;

typedef struct _i7z {
  int major_version;
  int minor_version;
  I7z_pack main_pack;
  I7z_pack additional_pack;
  unsigned char **additional_streams;
  UINT64 nfiles;
  UINT64 *file_to_substream;
  UINT64 *file_to_folder;
  unsigned short int **wc_filenames;
  char **filenames;
} I7z;

I7z *i7z_create(void);
I7z_stream *i7z_stream_create(void);
void i7z_stream_destroy(I7z_stream *);
void i7z_free(I7z *i7z);
int i7z_parse_header(I7z **i7z, I7z_stream *);
int i7z_identify(I7z *i7z, I7z_stream *);

#define i7z_stream_read(st, buf, c) (st)->read_func((st)->obj, buf, c)
#define i7z_stream_seek(st, off, w) (st)->seek_func((st)->obj, off, w)
#define i7z_stream_tell(st) (st)->tell_func((st)->obj)
#define i7z_stream_close(st) (st)->close_func((st)->obj)

#endif
