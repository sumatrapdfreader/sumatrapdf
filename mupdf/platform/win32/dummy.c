/* We put the font object files in the 'AdditionalDependencies' list,
 * but we need at least one C file to link the library.
 * Since we need different object files for 32 and 64 bit builds,
 * we can't just include them in the file list.
 */
int libresources_dummy;
