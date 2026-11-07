/* SPDX-License-Identifier: FTL OR GPL-2.0-or-later
 * the modules EPDF builds: TrueType, Type 1, CFF, the PostScript helpers,
 * the smooth and the monochrome rasterisers, and the auto-hinter */
FT_USE_MODULE( FT_Driver_ClassRec, tt_driver_class )
FT_USE_MODULE( FT_Driver_ClassRec, t1_driver_class )
FT_USE_MODULE( FT_Driver_ClassRec, cff_driver_class )
FT_USE_MODULE( FT_Module_Class, psaux_module_class )
FT_USE_MODULE( FT_Module_Class, psnames_module_class )
FT_USE_MODULE( FT_Module_Class, pshinter_module_class )
FT_USE_MODULE( FT_Module_Class, sfnt_module_class )
FT_USE_MODULE( FT_Renderer_Class, ft_smooth_renderer_class )
FT_USE_MODULE( FT_Renderer_Class, ft_raster1_renderer_class )
#ifndef EPDF_NO_AUTOFIT
FT_USE_MODULE( FT_Module_Class, autofit_module_class )
#endif
