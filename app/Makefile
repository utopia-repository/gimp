# Generated automatically from Makefile.in by configure.
# set up so stupid things
SHELL = /bin/sh
.SUFFIXES:
.SUFFIXES: .c .o

# set the MAKE variable if necessary


# set the compiler variable
CC		=	gcc

# set the various libraries that are needed
LIBS		=	$(DESIRED_MOTIF_LIB) -lXpm -lXmu -lXt  -lSM -lICE -lXext -lX11  -lm 

# remember to add the includes
X_CFLAGS	=	 -I/usr/X11R6/include
CFLAGS		=	-g -O2 -Wall $(X_CFLAGS)

# set the linker flags
X_LDFLAGS	=	-L/usr/X11R6/lib
LDFLAGS		=	 $(X_LDFLAGS)

# specify how depends are remade
MAKEDEPEND	=	$(CC) -MM

# the name of the program to create
PROGRAM	= gimp

# all of the sources
SRCS =				\
	main.c			\
	errors.c		\
				\
	actionarea.c		\
	app-procs.c		\
	buildmenu.c		\
	callbacks.c		\
	color_select.c		\
	fileops.c		\
	gimprc.c		\
	menus.c			\
	interface.c		\
	palette.c		\
	widget.c		\
				\
	colormaps.c		\
	disp-callbacks.c	\
	gconvert.c		\
	gdither.c 		\
	gimage.c		\
	gdisplay.c		\
	gdisplay_ops.c		\
	gximage.c		\
	image_buf.c		\
	info_dialog.c		\
	info_window.c		\
	scale.c			\
	scroll.c		\
	shadow_ops.c		\
	status.c		\
	view_ops.c		\
	visual.c		\
	workprocs.c		\
				\
	cursorutil.c		\
	general.c		\
	linked.c		\
	memutils.c		\
	progress.c		\
	timer.c			\
				\
	plug_in.c		\
	autodialog.c		\
				\
        bezier_select.c         \
        boundary.c              \
        crop.c                  \
        draw_core.c             \
        global_edit.c           \
        gregion.c               \
        edit_selection.c        \
        ellipse_select.c        \
        flip_tool.c             \
        free_select.c           \
        fuzzy_select.c          \
        iscissors.c             \
        rect_select.c           \
        rotate_tool.c           \
        scale_tool.c            \
        selection.c             \
        shear_tool.c            \
        temp_buf.c              \
        text_tool.c             \
        tools.c                 \
        transform_core.c        \
        transform_tool.c        \
        undo.c                  \
				\
	airbrush.c		\
	blend.c			\
	brushes.c		\
	brush_select.c		\
	bucket_fill.c		\
	clone.c			\
	color_picker.c		\
	convolve.c		\
	paintbrush.c		\
	paint_core.c		\
	paint_funcs.c		


# all of the objects
OBJS =	$(SRCS:.c=.o)

all: $(PROGRAM)


$(PROGRAM): $(OBJS)
	$(CC) $(OBJS) $(LINCLUDE) $(LIBS) $(LDFLAGS) -o $(PROGRAM)

depend:
	$(MAKEDEPEND) $(CFLAGS) $(SRCS) > .depend

.depend:
	touch .depend

clean:
	rm -f *.o *~ $(PROGRAM) gimp-static gimp-dynamic

include .depend
