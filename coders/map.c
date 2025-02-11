/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%                            M   M   AAA   PPPP                               %
%                            MM MM  A   A  P   P                              %
%                            M M M  AAAAA  PPPP                               %
%                            M   M  A   A  P                                  %
%                            M   M  A   A  P                                  %
%                                                                             %
%                                                                             %
%                  Read/Write Image Colormaps as an Image File.               %
%                                                                             %
%                              Software Design                                %
%                                   Cristy                                    %
%                                 July 1992                                   %
%                                                                             %
%                                                                             %
%  Copyright 1999 ImageMagick Studio LLC, a non-profit organization           %
%  dedicated to making software imaging solutions freely available.           %
%                                                                             %
%  You may not use this file except in compliance with the License.  You may  %
%  obtain a copy of the License at                                            %
%                                                                             %
%    https://imagemagick.org/script/license.php                               %
%                                                                             %
%  Unless required by applicable law or agreed to in writing, software        %
%  distributed under the License is distributed on an "AS IS" BASIS,          %
%  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   %
%  See the License for the specific language governing permissions and        %
%  limitations under the License.                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%
*/

/*
  Include declarations.
*/
#include "magick/studio.h"
#include "magick/attribute.h"
#include "magick/blob.h"
#include "magick/blob-private.h"
#include "magick/cache.h"
#include "magick/color.h"
#include "magick/color-private.h"
#include "magick/colormap.h"
#include "magick/colormap-private.h"
#include "magick/colorspace.h"
#include "magick/colorspace-private.h"
#include "magick/exception.h"
#include "magick/exception-private.h"
#include "magick/histogram.h"
#include "magick/image.h"
#include "magick/image-private.h"
#include "magick/list.h"
#include "magick/magick.h"
#include "magick/memory_.h"
#include "magick/pixel-accessor.h"
#include "magick/quantum-private.h"
#include "magick/static.h"
#include "magick/statistic.h"
#include "magick/string_.h"
#include "magick/module.h"

/*
  Forward declarations.
*/
static MagickBooleanType
  WriteMAPImage(const ImageInfo *,Image *);

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   R e a d M A P I m a g e                                                   %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  ReadMAPImage() reads an image of raw RGB colormap and colormap index
%  bytes and returns it.  It allocates the memory necessary for the new Image
%  structure and returns a pointer to the new image.
%
%  The format of the ReadMAPImage method is:
%
%      Image *ReadMAPImage(const ImageInfo *image_info,ExceptionInfo *exception)
%
%  A description of each parameter follows:
%
%    o image_info: the image info.
%
%    o exception: return any errors or warnings in this structure.
%
*/
static Image *ReadMAPImage(const ImageInfo *image_info,ExceptionInfo *exception)
{
  Image
    *image;

  IndexPacket
    index;

  MagickBooleanType
    status;

  IndexPacket
    *indexes;

  ssize_t
    x;

  PixelPacket
    *q;

  ssize_t
    i;

  unsigned char
    *p;

  size_t
    depth,
    packet_size,
    quantum;

  ssize_t
    count,
    y;

  unsigned char
    *colormap,
    *pixels;

  /*
    Open image file.
  */
  assert(image_info != (const ImageInfo *) NULL);
  assert(image_info->signature == MagickCoreSignature);
  assert(exception != (ExceptionInfo *) NULL);
  assert(exception->signature == MagickCoreSignature);
  if (IsEventLogging() != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",
      image_info->filename);
  image=AcquireImage(image_info);
  if ((image->columns == 0) || (image->rows == 0))
    ThrowReaderException(OptionError,"MustSpecifyImageSize");
  status=OpenBlob(image_info,image,ReadBinaryBlobMode,exception);
  if (status == MagickFalse)
    {
      image=DestroyImageList(image);
      return((Image *) NULL);
    }
  /*
    Initialize image structure.
  */
  image->storage_class=PseudoClass;
  status=AcquireImageColormap(image,(size_t)
    (image->offset != 0 ? image->offset : 256));
  if (status == MagickFalse)
    ThrowReaderException(ResourceLimitError,"MemoryAllocationFailed");
  depth=GetImageQuantumDepth(image,MagickTrue);
  packet_size=(size_t) (depth/8);
  pixels=(unsigned char *) AcquireQuantumMemory(image->columns,packet_size*
    sizeof(*pixels));
  packet_size=(size_t) (depth > 8 ? 6UL : 3UL);
  colormap=(unsigned char *) AcquireQuantumMemory(image->colors,packet_size*
    sizeof(*colormap));
  if ((pixels == (unsigned char *) NULL) ||
      (colormap == (unsigned char *) NULL))
    {
      pixels=(unsigned char *) RelinquishMagickMemory(pixels);
      colormap=(unsigned char *) RelinquishMagickMemory(colormap);
      ThrowReaderException(ResourceLimitError,"MemoryAllocationFailed");
    }
  /*
    Read image colormap.
  */
  count=ReadBlob(image,packet_size*image->colors,colormap);
  if (count != (ssize_t) (packet_size*image->colors))
    {
      pixels=(unsigned char *) RelinquishMagickMemory(pixels);
      colormap=(unsigned char *) RelinquishMagickMemory(colormap);
      ThrowReaderException(CorruptImageError,"InsufficientImageDataInFile");
    }
  p=colormap;
  if (image->depth <= 8)
    for (i=0; i < (ssize_t) image->colors; i++)
    {
      image->colormap[i].red=ScaleCharToQuantum(*p++);
      image->colormap[i].green=ScaleCharToQuantum(*p++);
      image->colormap[i].blue=ScaleCharToQuantum(*p++);
    }
  else
    for (i=0; i < (ssize_t) image->colors; i++)
    {
      quantum=(*p++ << 8);
      quantum|=(*p++);
      image->colormap[i].red=(Quantum) quantum;
      quantum=(*p++ << 8);
      quantum|=(*p++);
      image->colormap[i].green=(Quantum) quantum;
      quantum=(*p++ << 8);
      quantum|=(*p++);
      image->colormap[i].blue=(Quantum) quantum;
    }
  colormap=(unsigned char *) RelinquishMagickMemory(colormap);
  if (image_info->ping != MagickFalse)
    {
      (void) CloseBlob(image);
      pixels=(unsigned char *) RelinquishMagickMemory(pixels);
      return(GetFirstImageInList(image));
    }
  status=SetImageExtent(image,image->columns,image->rows);
  if (status == MagickFalse)
    {
      pixels=(unsigned char *) RelinquishMagickMemory(pixels);
      InheritException(exception,&image->exception);
      return(DestroyImageList(image));
    }
  /*
    Read image pixels.
  */
  packet_size=(size_t) (depth/8);
  for (y=0; y < (ssize_t) image->rows; y++)
  {
    p=pixels;
    q=QueueAuthenticPixels(image,0,y,image->columns,1,exception);
    if (q == (PixelPacket *) NULL)
      break;
    indexes=GetAuthenticIndexQueue(image);
    count=ReadBlob(image,(size_t) packet_size*image->columns,pixels);
    if (count != (ssize_t) (packet_size*image->columns))
      break;
    for (x=0; x < (ssize_t) image->columns; x++)
    {
      index=ConstrainColormapIndex(image,*p);
      p++;
      if (image->colors > 256)
        {
          index=ConstrainColormapIndex(image,((size_t) index << 8)+(*p));
          p++;
        }
      SetPixelIndex(indexes+x,index);
      SetPixelRGBO(q,image->colormap+(ssize_t) index);
    }
    if (SyncAuthenticPixels(image,exception) == MagickFalse)
      break;
  }
  pixels=(unsigned char *) RelinquishMagickMemory(pixels);
  if (y < (ssize_t) image->rows)
    ThrowFileException(exception,CorruptImageError,"UnexpectedEndOfFile",
      image->filename);
  (void) CloseBlob(image);
  return(GetFirstImageInList(image));
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   R e g i s t e r M A P I m a g e                                           %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  RegisterMAPImage() adds attributes for the MAP image format to
%  the list of supported formats.  The attributes include the image format
%  tag, a method to read and/or write the format, whether the format
%  supports the saving of more than one frame to the same file or blob,
%  whether the format supports native in-memory I/O, and a brief
%  description of the format.
%
%  The format of the RegisterMAPImage method is:
%
%      size_t RegisterMAPImage(void)
%
*/
ModuleExport size_t RegisterMAPImage(void)
{
  MagickInfo
    *entry;

  entry=SetMagickInfo("MAP");
  entry->decoder=(DecodeImageHandler *) ReadMAPImage;
  entry->encoder=(EncodeImageHandler *) WriteMAPImage;
  entry->adjoin=MagickFalse;
  entry->format_type=ExplicitFormatType;
  entry->raw=MagickTrue;
  entry->endian_support=MagickTrue;
  entry->description=ConstantString("Colormap intensities and indices");
  entry->magick_module=ConstantString("MAP");
  (void) RegisterMagickInfo(entry);
  return(MagickImageCoderSignature);
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   U n r e g i s t e r M A P I m a g e                                       %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  UnregisterMAPImage() removes format registrations made by the
%  MAP module from the list of supported formats.
%
%  The format of the UnregisterMAPImage method is:
%
%      UnregisterMAPImage(void)
%
*/
ModuleExport void UnregisterMAPImage(void)
{
  (void) UnregisterMagickInfo("MAP");
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   W r i t e M A P I m a g e                                                 %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  WriteMAPImage() writes an image to a file as red, green, and blue
%  colormap bytes followed by the colormap indexes.
%
%  The format of the WriteMAPImage method is:
%
%      MagickBooleanType WriteMAPImage(const ImageInfo *image_info,Image *image)
%
%  A description of each parameter follows.
%
%    o image_info: the image info.
%
%    o image:  The image.
%
%
*/
static MagickBooleanType WriteMAPImage(const ImageInfo *image_info,Image *image)
{
  MagickBooleanType
    status;

  const IndexPacket
    *indexes;

  const PixelPacket
    *p;

  ssize_t
    i,
    x;

  unsigned char
    *q;

  size_t
    depth,
    packet_size;

  ssize_t
    y;

  unsigned char
    *colormap,
    *pixels;

  /*
    Open output image file.
  */
  assert(image_info != (const ImageInfo *) NULL);
  assert(image_info->signature == MagickCoreSignature);
  assert(image != (Image *) NULL);
  assert(image->signature == MagickCoreSignature);
  if (IsEventLogging() != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  status=OpenBlob(image_info,image,WriteBinaryBlobMode,&image->exception);
  if (status == MagickFalse)
    return(status);
  if (IssRGBCompatibleColorspace(image->colorspace) == MagickFalse)
    (void) TransformImageColorspace(image,sRGBColorspace);
  /*
    Allocate colormap.
  */
  if (SetImageType(image,PaletteType) == MagickFalse)
    ThrowWriterException(ResourceLimitError,"MemoryAllocationFailed");
  depth=GetImageQuantumDepth(image,MagickTrue);
  packet_size=(size_t) (depth/8);
  pixels=(unsigned char *) AcquireQuantumMemory(image->columns,packet_size*
    sizeof(*pixels));
  packet_size=(size_t) (image->colors > 256 ? 6UL : 3UL);
  colormap=(unsigned char *) AcquireQuantumMemory(image->colors,packet_size*
    sizeof(*colormap));
  if ((pixels == (unsigned char *) NULL) ||
      (colormap == (unsigned char *) NULL))
    {
      if (colormap != (unsigned char *) NULL)
        colormap=(unsigned char *) RelinquishMagickMemory(colormap);
      if (pixels != (unsigned char *) NULL)
        pixels=(unsigned char *) RelinquishMagickMemory(pixels);
      ThrowWriterException(ResourceLimitError,"MemoryAllocationFailed");
    }
  /*
    Write colormap to file.
  */
  q=colormap;
  if (image->colors <= 256)
    for (i=0; i < (ssize_t) image->colors; i++)
    {
      *q++=(unsigned char) ScaleQuantumToChar(image->colormap[i].red);
      *q++=(unsigned char) ScaleQuantumToChar(image->colormap[i].green);
      *q++=(unsigned char) ScaleQuantumToChar(image->colormap[i].blue);
    }
  else
    for (i=0; i < (ssize_t) image->colors; i++)
    {
      *q++=(unsigned char) (ScaleQuantumToShort(image->colormap[i].red) >> 8);
      *q++=(unsigned char) (ScaleQuantumToShort(image->colormap[i].red) & 0xff);
      *q++=(unsigned char) (ScaleQuantumToShort(image->colormap[i].green) >> 8);
      *q++=(unsigned char) (ScaleQuantumToShort(image->colormap[i].green) &
        0xff);
      *q++=(unsigned char) (ScaleQuantumToShort(image->colormap[i].blue) >> 8);
      *q++=(unsigned char) (ScaleQuantumToShort(image->colormap[i].blue) &
        0xff);
    }
  (void) WriteBlob(image,packet_size*image->colors,colormap);
  colormap=(unsigned char *) RelinquishMagickMemory(colormap);
  /*
    Write image pixels to file.
  */
  for (y=0; y < (ssize_t) image->rows; y++)
  {
    p=GetVirtualPixels(image,0,y,image->columns,1,&image->exception);
    if (p == (const PixelPacket *) NULL)
      break;
    indexes=GetVirtualIndexQueue(image);
    q=pixels;
    for (x=0; x < (ssize_t) image->columns; x++)
    {
      if (image->colors > 256)
        *q++=(unsigned char) ((size_t) GetPixelIndex(indexes+x) >> 8);
      *q++=(unsigned char) ((size_t) GetPixelIndex(indexes+x));
    }
    (void) WriteBlob(image,(size_t) (q-pixels),pixels);
  }
  pixels=(unsigned char *) RelinquishMagickMemory(pixels);
  (void) CloseBlob(image);
  return(status);
}
