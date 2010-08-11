/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef __RESOURCE_H__
#define __RESOURCE_H__


#define DEPTH                0
#define DITHERTYPE           0	/*  ordered dither  */
#define COLORCUBE            "7.6.4"
#define NOMITSHM             "False"
#define THRESHOLD            15
#define LEVELS_OF_UNDO       10
#define BYTES_OF_UNDO        15000000
#define ARROW_ACCEL          25
#define BITMAP_DIR           "../bitmap"
#define GIMPRC_SEARCH_PATH   "~/:/usr/local/lib/gimp"
#define USE_BBOX             "True"
#define STINGY               "True"
#define QUICK_16BIT          "True"
#define INSIGNIA             "/usr/openwin/include/X11/bitmaps/xlogo32"


#define GimpNdefaultDepth               "defaultDepth"
#define GimpCDefaultDepth               "DefaultDepth"
#define GimpNditherType                 "ditherType"
#define GimpCDitherType                 "DitherType"
#define GimpNcolorCube			"colorCube"
#define GimpCColorCube			"ColorCube"
#define GimpNmitShm                     "mitShm"
#define GimpCMitShm                     "MitShm"
#define GimpNthreshold                  "threshold"
#define GimpCThreshold                  "Threshold"
#define GimpNlevelsOfUndo               "levelsOfUndo"
#define GimpCLevelsOfUndo               "LevelsOfUndo"
#define GimpNbytesOfUndo                "bytesOfUndo"
#define GimpCBytesOfUndo                "BytesOfUndo"
#define GimpNarrowAccel                 "arrowAccel"
#define GimpCArrowAccel                 "ArrowAccel"
#define GimpNbitmapDir                  "bitmapDir"
#define GimpCBitmapDir                  "BitmapDir"
#define GimpNgimprcSearchPath           "gimprcSearchPath"
#define GimpCGimprcSearchPath           "GimprcSearchPath"
#define GimpNuseBBox                    "useBBox"
#define GimpCUseBBox                    "UseBBox"
#define GimpNstingy                     "stingy"
#define GimpCStingy                     "Stingy"
#define GimpNquick16Bit                 "quick16Bit"
#define GimpCQuick16Bit                 "Quick16Bit"

#define GimpNinsignia                   "insignia"
#define GimpCInsignia                   "Insignia"


STATIC XtResource resources[] =
{
  {
    GimpNdefaultDepth, GimpCDefaultDepth,
    XmRInt, sizeof (int),
    XtOffsetOf (appdata, depth), XmRImmediate,
    (XtPointer) DEPTH,
  },
  {
    GimpNditherType, GimpCDitherType,
    XmRInt, sizeof (int),
    XtOffsetOf (appdata, dither_type), XmRImmediate,
    (XtPointer) DITHERTYPE,
  },
  {
    GimpNcolorCube, GimpCColorCube,
    XmRString, sizeof (String),
    XtOffsetOf (appdata, colorcube), XmRImmediate,
    (XtPointer) COLORCUBE,
  },
  {
    GimpNmitShm, GimpCMitShm,
    XmRBoolean, sizeof (Boolean),
    XtOffsetOf (appdata, mit_shm), XmRImmediate,
    (XtPointer) True,
  },
  {
    GimpNthreshold, GimpCThreshold,
    XmRInt, sizeof (int),
    XtOffsetOf (appdata, threshold), XmRImmediate,
    (XtPointer) THRESHOLD,
  },
  {
    GimpNlevelsOfUndo, GimpCLevelsOfUndo,
    XmRInt, sizeof (int),
    XtOffsetOf (appdata, levels_of_undo), XmRImmediate,
    (XtPointer) LEVELS_OF_UNDO,
  },
  {
    GimpNbytesOfUndo, GimpCBytesOfUndo,
    XmRInt, sizeof (int),
    XtOffsetOf (appdata, bytes_of_undo), XmRImmediate,
    (XtPointer) BYTES_OF_UNDO,
  },
  {
    GimpNarrowAccel, GimpCArrowAccel,
    XmRInt, sizeof (int),
    XtOffsetOf (appdata, arrow_accel), XmRImmediate,
    (XtPointer) ARROW_ACCEL,
  },
  {
    GimpNbitmapDir, GimpCBitmapDir,
    XmRString, sizeof (String),
    XtOffsetOf (appdata, bitmap_dir), XmRImmediate,
    (XtPointer) BITMAP_DIR,
  },
  {
    GimpNgimprcSearchPath, GimpCGimprcSearchPath,
    XmRString, sizeof (String),
    XtOffsetOf (appdata, gimprc_search_path), XmRImmediate,
    (XtPointer) GIMPRC_SEARCH_PATH,
  },
  {
    GimpNuseBBox, GimpCUseBBox,
    XmRBoolean, sizeof (Boolean),
    XtOffsetOf (appdata, use_bbox), XmRImmediate,
    (XtPointer) False,
  },
  {
    GimpNstingy, GimpCStingy,
    XmRBoolean, sizeof (Boolean),
    XtOffsetOf (appdata, stingy), XmRImmediate,
    (XtPointer) False,
  },
  {
    GimpNquick16Bit, GimpCQuick16Bit,
    XmRBoolean, sizeof (Boolean),
    XtOffsetOf (appdata, quick_16bit), XmRImmediate,
    (XtPointer) False,
  },
  {
    GimpNinsignia, GimpCInsignia,
    XmRString, sizeof (String),
    XtOffsetOf (appdata, insignia), XmRImmediate,
    (XtPointer) INSIGNIA,
  },
};

STATIC String fallbacks[] =
{
  "*Title: The Gimp",
  "*background: #bebebe", /* <-- a light grey color */
  "*foreground: black",
  "*fontList: -*-new century schoolbook-bold-r-*-*-12-*-*-*-*-*-*-*",
  "*highlightThickness: 1",

	/*  geometries  */
  "Gimp.geometry: +10+10",
  "*openFileDialog_popup.geometry: +475+250",
  "*saveFileDialog_popup.geometry: +475+250",
  "*cropInfoDialog.geometry: +10+200",
  "*colorPickerInfoDialog.geometry: +10+200",
  "*scaleInfoDialog.geometry: +10+200",
  "*rotateInfoDialog.geometry: +10+200",
  "*progressDialog.geometry: +300+10",
  "*brushSelectDialog.geometry: +10+250",
  "*paletteDialog.geometry: +300+10",
  "*colorSelectDialog.geometry: +320+30",

  "*toolFrameLabel.labelString: Toolbox",
  "*previewFrameLabel.labelString: Brush Preview",
  "*fileMenu.labelString: File",
  "*editMainMenu.labelString: Edit",
  "*editMenu.labelString: Edit",
  "*selectMenu.labelString: Select",
  "*viewMenu.labelString: View",
  "*toolMenu.labelString: Tools",
  "*brushMenu.labelString: Brushes",
  "*colorMenu.labelString: Color",
  "*filterMenu.labelString: Filters",

  "*saveFileDialog.dialogTitle: Save File",
  "*openFileDialog.dialogTitle: Open File",
  "*saveasFileDialog.dialogTitle: Save File As",

  "*file_new.labelString: New",
  "*file_new.mnemonic: N",
  "*file_new.accelerator: Ctrl<Key>N",
  "*file_new.acceleratorText: Ctl+N",

  "*file_open.labelString: Open...",
  "*file_open.mnemonic: O",
  "*file_open.accelerator: Ctrl<Key>O",
  "*file_open.acceleratorText: Ctl+O",

  "*file_save.labelString: Save...",
  "*file_save.mnemonic: S",
  "*file_save.accelerator: Ctrl<Key>S",
  "*file_save.acceleratorText: Ctl+S",

  "*file_exit.labelString: Quit",
  "*file_exit.mnemonic: Q",
  "*file_exit.accelerator: Ctrl<Key>Q",
  "*file_exit.acceleratorText: Ctl+Q",

  "*edit_cut.labelString: Cut",
  "*edit_cut.mnemonic: t",
  "*edit_cut.accelerator: Ctrl<Key>X",
  "*edit_cut.acceleratorText: Ctl+X",

  "*edit_copy.labelString: Copy",
  "*edit_copy.mnemonic: C",
  "*edit_copy.accelerator: Ctrl<Key>C",
  "*edit_copy.acceleratorText: Ctl+C",

  "*edit_paste.labelString: Paste",
  "*edit_paste.mnemonic: P",
  "*edit_paste.accelerator: Ctrl<Key>V",
  "*edit_paste.acceleratorText: Ctl+V",

  "*edit_clear.labelString: Clear",
  "*edit_clear.mnemonic: e",
  "*edit_clear.accelerator: Ctrl<Key>K",
  "*edit_clear.acceleratorText: Ctl+K",

  "*named_cut.labelString: Cut to Buffer",
  "*named_cut.accelerator: Shift Ctrl<Key>X",
  "*named_cut.acceleratorText: Shft+Ctl+X",

  "*named_copy.labelString: Copy to Buffer",
  "*named_copy.accelerator: Shift Ctrl<Key>C",
  "*named_copy.acceleratorText: Shft+Ctl+C",

  "*named_paste.labelString: Paste Buffer",
  "*named_paste.accelerator: Shift Ctrl<Key>V",
  "*named_paste.acceleratorText: Shft+Ctl+V",

  "*edit_undo.labelString: Undo",
  "*edit_undo.mnemonic: U",
  "*edit_undo.accelerator: Ctrl<Key>Z",
  "*edit_undo.acceleratorText: Ctl+Z",

  "*edit_redo.labelString: Redo",
  "*edit_redo.mnemonic: R",
  "*edit_redo.accelerator: Ctrl<Key>R",
  "*edit_redo.acceleratorText: Ctl+R",

  "*select_invert.labelString: Select Invert",
  "*select_invert.mnemonic: I",
  "*select_invert.accelerator: Ctrl<Key>I",
  "*select_invert.acceleratorText: Ctl+I",

  "*select_toggle.labelString: Select Toggle",
  "*select_toggle.mnemonic: T",
  "*select_toggle.accelerator: Ctrl<Key>T",
  "*select_toggle.acceleratorText: Ctl+T",

  "*select_all.labelString: Select All",
  "*select_all.mnemonic: A",
  "*select_all.accelerator: Ctrl<Key>A",
  "*select_all.acceleratorText: Ctl+A",

  "*select_none.labelString: Select None",
  "*select_none.mnemonic: N",
  "*select_none.accelerator: Shift Ctrl<Key>A",
  "*select_none.acceleratorText: Shft+Ctl+A",

  "*select_anchor.labelString: Select Anchor",
  "*select_anchor.accelerator: Shift Ctrl<Key>H",
  "*select_anchor.acceleratorText: Shft+Ctl+H",

  "*select_sharpen.labelString: Select Sharpen",
  "*select_sharpen.accelerator: Shift Ctrl<Key>S",
  "*select_sharpen.acceleratorText: Shft+Ctl+S",

  "*select_opacity.labelString: Paste Opacity...",
  "*select_opacity.accelerator: Shift Ctrl<Key>O",
  "*select_opacity.acceleratorText: Shft+Ctl+O",

  "*select_to_gdisp.labelString: Selection to Mask",
  "*select_to_gdisp.accelerator: Shift Ctrl<Key>T",
  "*select_to_gdisp.acceleratorText: Shft+Ctl+T",

  "*select_from_gdisp.labelString: Mask to Selection",
  "*select_from_gdisp.accelerator: Shift Ctrl<Key>F",
  "*select_from_gdisp.acceleratorText: Shft+Ctl+F",

  "*edit_options.labelString: Edit Options...",
  "*edit_options.mnemonic: E",
  "*edit_options.accelerator: Shift Ctrl<Key>E",
  "*edit_options.acceleratorText: Shft+Ctl+E",

  "*view_options.labelString: View Options...",
  "*view_options.mnemonic: V",

  "*info_window.labelString: Window Info...",
  "*info_window.mnemonic: W",

  "*zoom_in.labelString: Zoom In",
  "*zoom_in.mnemonic: I",
  "*zoom_in.accelerator: <Key>plus",
  "*zoom_in.acceleratorText: '+'",

  "*zoom_out.labelString: Zoom Out",
  "*zoom_out.mnemonic: O",
  "*zoom_out.accelerator: <Key>minus",
  "*zoom_out.acceleratorText: '-'",

  "*shrink_wrap.labelString: Shrink Wrap",
  "*shrink_wrap.mnemonic: S",
  "*shrink_wrap.accelerator: Ctrl<Key>E",
  "*shrink_wrap.acceleratorText: Ctl+E",

  "*close_window.labelString: Close Window",
  "*close_window.mnemonic: C",
  "*close_window.accelerator: Ctrl<Key>W",
  "*close_window.acceleratorText: Ctl+W",

  "*new_view.labelString: New View",
  "*new_view.mnemonic: N",
/*  "*new_view.accelerator: Ctrl<Key>N", */
/*  "*new_view.acceleratorText: Ctl+N", */

  "*rect_select.labelString: Rect Select",
  "*rect_select.accelerator: <Key>r",
  "*rect_select.acceleratorText: R",

  "*ellipse_select.labelString: Ellipse Select",
  "*ellipse_select.accelerator: <Key>e",
  "*ellipse_select.acceleratorText: E",

  "*free_select.labelString: Free Select",
  "*free_select.accelerator: <Key>f",
  "*free_select.acceleratorText: F",

  "*fuzzy_select.labelString: Fuzzy Select",
  "*fuzzy_select.accelerator: <Key>z",
  "*fuzzy_select.acceleratorText: Z",

  "*bezier_select.labelString: Bezier Select",
  "*bezier_select.accelerator: <Key>b",
  "*bezier_select.acceleratorText: B",

  "*iscissors.labelString: Intelligent Scissors",
  "*iscissors.accelerator: <Key>i",
  "*iscissors.acceleratorText: I",

  "*crop.labelString: Crop",
  "*crop.accelerator: Shift <Key>c",
  "*crop.acceleratorText: Shft+C",
  
  "*transform.labelString: Transform",
  "*transform.accelerator: Shift<Key>T",
  "*transform.acceleratorText: Shft+T",
  
  "*flip_horz.labelString: Horizontal Flip",
  "*flip_horz.accelerator: <Key>h",
  "*flip_horz.acceleratorText: H",
  
  "*flip_vert.labelString: Vertical Flip",
  "*flip_vert.accelerator: <Key>v",
  "*flip_vert.acceleratorText: V",

  "*color_picker.labelString: Color Picker",
  "*color_picker.accelerator: <Key>o",
  "*color_picker.acceleratorText: O",

  "*bucket_fill.labelString: Bucket Fill",
  "*bucket_fill.accelerator: Shift <Key>b",
  "*bucket_fill.acceleratorText: Shft+B",

  "*paintbrush.labelString: Paintbrush",
  "*paintbrush.accelerator: <Key>p",
  "*paintbrush.acceleratorText: P",

  "*airbrush.labelString: Airbrush",
  "*airbrush.accelerator: <Key>a",
  "*airbrush.acceleratorText: A",

  "*blend.labelString: Blend",
  "*blend.accelerator: <Key>l",
  "*blend.acceleratorText: L",

  "*convolve.labelString: Convolve",
  "*convolve.accelerator: <Key>v",
  "*convolve.acceleratorText: V",

  "*clone.labelString: Clone",
  "*clone.accelerator: <Key>c",
  "*clone.acceleratorText: C",

  "*text.labelString: Text",
  "*text.accelerator: <Key>t",
  "*text.acceleratorText: T",

  "*brushSelect.labelString: Select Brushes...",
  "*brushSelect.accelerator: Ctrl<Key>B",
  "*brushSelect.acceleratorText: Ctl+B",

  "*paletteDialog.labelString: Palette...",
  "*paletteDialog.accelerator: Ctrl<Key>P",
  "*paletteDialog.acceleratorText: Ctl+P",
  
  "*colorOptions.labelString: Color Options",
  "*ditherOptions.labelString: Dither Options",

  "*RGB.labelString: RGB",
  "*indexed.labelString: Indexed",
  "*greyscale.labelString: Greyscale",

  "*ordered.labelString: Ordered",
  "*floyd_steinberg.labelString: Floyd & Steinberg",

  NULL,
};


STATIC XrmOptionDescRec options[] =
{
  {"-depth",         "*defaultDepth",  XrmoptionSepArg,   NULL},
  {"-colorcube",     "*colorCube",     XrmoptionSepArg,   NULL},
  {"-bitmapdir",     "*bitmapDir",     XrmoptionSepArg,   NULL},
  {"-noshm",         "*mitShm",        XrmoptionNoArg,    NOMITSHM},
  {"-usebbox",       "*useBBox",       XrmoptionNoArg,    USE_BBOX},
  {"-stingy",        "*stingy",        XrmoptionNoArg,    STINGY},
  {"-quick16bit",    "*quick16Bit",    XrmoptionNoArg,    QUICK_16BIT},
};

#endif /* __RESOURCE_H__ */
