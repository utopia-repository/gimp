# the setup for the HP's on campus (UCB)
# INCLUDE	=	-I/usr/sww/include -I/home/tmp/spencer/include-hp
# LINCLUDE	=	-L/usr/sww/lib -L/home/tmp/spencer/lib-hp

# the setup for the SGI's on campus (UCB)
# INCLUDE	=	-I/usr/sww/include -I/home/tmp/spencer/include-sgi
# LINCLUDE	=	-L/usr/sww/lib -L/home/tmp/spencer/lib-sgi

# the setup for the Solaris machines at CEA
# INCLUDE	=       -I/usr/local/lib/jpeg/include -I../../include
# LINCLUDE	=       -L/usr/local/lib/jpeg -L../../lib

# the setup for linux at home
INCLUDE		=	-I/usr/X11R6/include -I/usr/local/include
LINCLUDE	=	-L/usr/X11R6/lib -L/usr/local/lib

# set the compiler variable
CC        	=	gcc

# specify if ranlib should be used
RANLIB		=	ranlib
#RANLIB		=	echo

# remember to add the includes
CFLAGS		=	-g -Wall $(INCLUDE)

# specify how depends are remade
MAKEDEPEND	=	gcc -MM

#########################################################
# You shouldn't have to modify anything below this line #
#########################################################

FILTERSRC = 	blur.c relief.c to-gray.c to-color.c to-indexed.c	\
		grayify.c bleed.c spread.c edge.c invert.c pixelize.c 	\
		flip_horz.c flip_vert.c shift.c enhance.c		\
		multiply.c difference.c subtract.c add.c		\
		duplicate.c offset.c blend.c composite.c		\
		gamma.c scale.c rotate.c tile.c	gauss.c			\
		compose.c decompose.c netpbm.c				\
		jpeg.c tiff.c gif.c png.c gbrush.c xpm.c tga.c

FILTEROBJ = $(FILTERSRC:.c=.o)
FILTERS = $(FILTERSRC:.c=)
LIBGIMP = libgimp.a

all: $(LIBGIMP) $(FILTERS)


# If you are using gmake use the following 2 rules...
# %: %.c $(LIBGIMP)
#	$(CC) $(CFLAGS) -o $@ $< $(LIBGIMP)
#
# %.o: %.c
#	$(CC) $(CFLAGS) -c $< -o $@

# If you are using make use the following 2 rules...
.c: $(LIBGIMP)
	$(CC) $(CFLAGS) -o $@ $< $(LIBGIMP)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@


gamma:  gamma.c $(LIBGIMP)
	-$(CC) $(CFLAGS) $(LINCLUDE) -o gamma gamma.c $(LIBGIMP) -lc -lm

rotate: rotate.c $(LIBGIMP)
	-$(CC) $(CFLAGS) $(LINCLUDE) -o rotate rotate.c $(LIBGIMP) -lc -lm

gauss: gauss.c $(LIBGIMP)
	-$(CC) $(CFLAGS) $(LINCLUDE) -o gauss gauss.c $(LIBGIMP) -lc -lm

jpeg: jpeg.c $(LIBGIMP)
	-$(CC) $(CFLAGS) $(LINCLUDE) -o jpeg jpeg.c $(LIBGIMP) -ljpeg

png: png.c $(LIBGIMP)
	-$(CC) $(CFLAGS) $(LINCLUDE) -o png png.c $(LIBGIMP) -lpng -lz -lc -lm

tiff: tiff.c $(LIBGIMP)
	-$(CC) $(CFLAGS) $(LINCLUDE) -o tiff tiff.c $(LIBGIMP) -ltiff -ljpeg -lc -lm

xpm: xpm.c $(LIBGIMP)
	-$(CC) $(CFLAGS) $(LINCLUDE) -o xpm xpm.c $(LIBGIMP) -lXpm -lX11 -lc

$(LIBGIMP): gimp.o
	ar rc $@ gimp.o
	$(RANLIB) $@

depend:
	$(MAKEDEPEND) $(INCLUDE) $(CFLAGS) $(FILTERSRC) > .depend

clean:
	rm -f *.o *~ $(LIBGIMP) $(FILTERS)

include .depend
