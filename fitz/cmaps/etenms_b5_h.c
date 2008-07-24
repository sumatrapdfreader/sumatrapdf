#ifdef USE_ETENMS_B5_H

#ifdef INCLUDE_CMAP_DATA

static const pdf_range g_cmap_etenms_b5_h_ranges[1] = {
 {32, 126, 1, 1}
};

static fz_error *new_etenms_b5_h(pdf_cmap **out)
{
	fz_error *error;
	pdf_cmap *cmap;
	error = pdf_newcmap(&cmap);
	if (error)
		return error;
	cmap->staticdata = 1;
	cmap->ranges = (pdf_range*)&g_cmap_etenms_b5_h_ranges[0];
	cmap->table = 0;
	strcpy(cmap->cmapname, "ETenms-B5-H");
	strcpy(cmap->usecmapname, "ETen-B5-H");
	cmap->wmode = 0;
	cmap->ncspace = 0;
	
	cmap->rlen = 1;
	cmap->rcap = 1;
	cmap->tlen = 0;
	cmap->tcap = 0;
	*out = cmap;

	return fz_okay;
}

#else

	if (!strcmp(name, "ETenms-B5-H"))
		return new_etenms_b5_h(cmapp);

#endif
#endif
