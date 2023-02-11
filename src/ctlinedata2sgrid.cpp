/*
This source code file is licensed under the GNU GPL Version 2.0 Licence by the following copyright holder:
Crown Copyright Commonwealth of Australia (Geoscience Australia) 2015.
The GNU GPL 2.0 licence is available at: http://www.gnu.org/licenses/gpl-2.0.html. If you require a paper copy of the GNU GPL 2.0 Licence, please write to Free Software Foundation, Inc. 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

Author: Ross C. Brodie, Geoscience Australia.
*/

#include <math.h>
#include <algorithm>
#include <numeric>
#include <vector>

#include "gaaem_version.h"
#include "general_types.h"
#include "general_utils.h"
#include "file_utils.h"
#include "blocklanguage.h"
#include "geometry3d.h"
#include "stopwatch.h"

#include "ticpp.h"
using namespace ticpp;

class cLogger glog; //The global instance of the log file manager
class cStackTrace gtrace; //The global instance of the stacktrace

class cSGridCreator{

private:	
	cBlock mControl;
	bool   binary = true;
	std::string sgriddir;
	std::string sgridprefix;
	std::string sgridsuffix;

	int subsample,lcol, xcol, ycol, ecol, crcol1, crcol2, tcol1, tcol2;
	bool haveresistivity;
	bool usedepthextent;
	double depthextent;
	int linenumber;
	int nlayers;
	int nsamples;
	bool usecellalignment = false;
	double cellwidth = 0;

	bool isconstantthickness;
	std::vector<double> constantthickness;

	std::vector<double> x;
	std::vector<double> y;
	std::vector<double> e;
	std::vector<std::vector<double>> c;
	std::vector<std::vector<double>> z;
	
	double InputConductivityScaling;
	double NullInputConductivity;
	double NullOutputProperty;
	double NullBelowElevation;
	double NullBelowDepth;

public:	

	cSGridCreator(const cBlock& control){		
		usedepthextent = false;
		mControl = control;
		getinputoptions();
		getsgridoptions();
	}

	void process(const std::string& filename){
		readdatafile(filename);		
		if (usecellalignment){
			create_sgrid_prop_alignment_cells();
		}
		else{
			create_sgrid_prop_alignment_points();
		}
	}

	void getsgridoptions(){

		cBlock b = mControl.findblock("SGrid");	
		binary      = b.getboolvalue("Binary");
		sgriddir    = b.getstringvalue("OutDir");
		sgridprefix = b.getstringvalue("Prefix");
		sgridsuffix = b.getstringvalue("Suffix");
				
		NullBelowElevation = b.getdoublevalue("NullBelowElevation");
		if (!isdefined(NullBelowElevation)){
			NullBelowElevation = -DBL_MAX;
		}

		NullBelowDepth = b.getdoublevalue("NullBelowDepth");
		if (!isdefined(NullBelowDepth)){
			NullBelowDepth = DBL_MAX;
		}
		
		NullOutputProperty = b.getdoublevalue("NullOutputProperty");		
		if (!isdefined(NullOutputProperty)){
			NullOutputProperty = -999;
		}

		usecellalignment = b.getboolvalue("UseCellAlignment");
		if (usecellalignment){
			cellwidth = b.getdoublevalue("CellWidth");
		}
	}

	void getinputoptions(){

		cBlock b = mControl.findblock("Input");
		subsample = b.getintvalue("Subsample");
		if (!isdefined(subsample))subsample = 1;

		std::string lstr = b.getstringvalue("Line");
		std::string xstr = b.getstringvalue("Easting");
		std::string ystr = b.getstringvalue("Northing");
		std::string estr = b.getstringvalue("Elevation");
		
		haveresistivity = false;
		std::string crstr = b.getstringvalue("Conductivity");
		if (!isdefined(crstr)){
			crstr = b.getstringvalue("Resistivity");
			haveresistivity = true;
		}
		

		InputConductivityScaling=1.0;
		std::string cunits = b.getstringvalue("InputConductivityUnits");
		if (!isdefined(cunits)){
			InputConductivityScaling = 1.0;
		}
		else if(strcasecmp(cunits,"S/m") == 0){
			InputConductivityScaling = 1.0;
		}
		else if(strcasecmp(cunits,"mS/m") == 0){
			InputConductivityScaling = 0.001;
		}		
		else{
			glog.logmsg("Unknown InputConductivityUnits %s\n",cunits.c_str());			
		}
		
		NullInputConductivity = b.getdoublevalue("NullInputConductivity");
		if (!isdefined(NullInputConductivity)){
			NullInputConductivity = -9999;
		}
		
		sscanf(lstr.c_str(), "Column %d", &lcol); lcol--;
		sscanf(xstr.c_str(), "Column %d", &xcol); xcol--;
		sscanf(ystr.c_str(), "Column %d", &ycol); ycol--;
		sscanf(estr.c_str(), "Column %d", &ecol); ecol--;
		sscanf(crstr.c_str(), "Column %d-%d", &crcol1, &crcol2); crcol1--; crcol2--;
		nlayers = crcol2 - crcol1 + 1;
		
		std::string tstr = b.getstringvalue("Thickness");
		if(sscanf(tstr.c_str(), "Column %d-%d", &tcol1, &tcol2) == 2){
			isconstantthickness = false;
			tcol1--; tcol2--;				
		}
		else{			
			isconstantthickness = true;
			constantthickness = b.getdoublevector("Thickness");
			tcol1 = 0; tcol2 = 0;			
			if(constantthickness.size() == 0){
				glog.errormsg(_SRC_,"Thickness not set\n");
			}
			else if((int)constantthickness.size() > 1 && (int)constantthickness.size() < (int) nlayers - 1){
				glog.errormsg(_SRC_,"Thickness not set correctly\n");
			}
			else if(constantthickness.size() == 1){
				constantthickness = std::vector<double>(nlayers-1, constantthickness[0]);
			}
			else{
				//all good
			}
			
		}		
	}
	
	void readdatafile(const std::string filename)	{
		
		FILE* fp = fileopen(filename, "r");
		std::string str;
		std::vector<std::vector<double>> M;
		int k=0;
		while (filegetline(fp, str)){			
			if(k%subsample == 0){
				M.push_back(getdoublevector(str.c_str(), " "));
			}	
			k++;
		}
		fclose(fp);
		
		nsamples = (int)M.size();
		linenumber = (int)M[0][lcol];

		x.resize(nsamples);
		y.resize(nsamples);
		e.resize(nsamples);
		c.resize(nsamples);
		z.resize(nsamples);	
		//////////
		for (int si = 0; si < nsamples; si++){
			x[si] = M[si][xcol];
			y[si] = M[si][ycol];
			e[si] = M[si][ecol];

			c[si].resize(nlayers);
			for (int li = 0; li < nlayers; li++){
				c[si][li] = M[si][crcol1 + li];				
				if(c[si][li]  != NullInputConductivity){
					if (haveresistivity) c[si][li] = 1.0 / c[si][li];
				   c[si][li] *= InputConductivityScaling;
				}
			}

			z[si].resize(nlayers+1);			
			z[si][0] = e[si];
			for (int li = 0; li < nlayers; li++){				
				
				double t;
				if (li < nlayers - 1){
					if (isconstantthickness == true){
						t = constantthickness[li];
					}
					else{
						t = M[si][tcol1 + li];						
					}
				}		
				else{
					if (isconstantthickness == true){
						t = constantthickness[li-1];
					}
					else{
						t = M[si][tcol1 + li - 1];
					}					
				}	
				
				z[si][li + 1] = z[si][li] - t;
				if(usedepthextent){
					if(z[si][li + 1] < (e[si] - depthextent)){
						z[si][li + 1] =  e[si] - depthextent;
					}
				}

			}
		}				
	}
	
	void create_sgrid_prop_alignment_cells(){	
		
		std::string sgriddatafile = sgridname() + ".sg.data";
		std::string sgriddatapath = sgriddir + sgridname() + ".sg.data";
		std::string sgridhdrpath  = sgriddir + sgridname() + ".sg";
		FILE* fp_data = fileopen(sgriddatapath, "w");
		fprintf(fp_data, "*\n");
		fprintf(fp_data, "*   X   Y   Z  Conductivity I   J   K\n");
		fprintf(fp_data, "*\n");

		for (int wi = 0; wi < 2; wi++){
			for (int li = 0; li <= nlayers; li++){				
				for (int si = 0; si <= nsamples; si++){
					double xc, yc, zc, ec, c0;					
					cVec v;
					if (si>0 && si<nsamples){
						ec = (e[si - 1] + e[si]) / 2.0;
						xc = (x[si - 1] + x[si]) / 2.0;
						yc = (y[si - 1] + y[si]) / 2.0;
						zc = (z[si - 1][li] + z[si][li]) / 2.0;							
						v = cVec(x[si]-x[si-1],y[si]-y[si-1],0.0);
					}
					else if (si == 0){//first cell
						ec = e[0] - (e[1] - e[0]) / 2.0;
						xc = x[0] - (x[1] - x[0]) / 2.0;
						yc = y[0] - (y[1] - y[0]) / 2.0;						
						zc = z[0][li] - (z[1][li] - z[0][li]) / 2.0;
						v = cVec(x[1]-x[0],y[1]-y[0],0.0);
					}
					else if (si == nsamples){//last cell
						ec = e[nsamples - 1] + (e[nsamples - 1] - e[nsamples - 2]) / 2.0;
						xc = x[nsamples - 1] + (x[nsamples - 1] - x[nsamples - 2]) / 2.0;
						yc = y[nsamples - 1] + (y[nsamples - 1] - y[nsamples - 2]) / 2.0;
						zc = z[nsamples - 1][li] + (z[nsamples - 1][li] - z[nsamples - 2][li]) / 2.0;
						v = cVec(x[nsamples-1]-x[nsamples-2],y[nsamples-1]-y[nsamples-2],0.0);
					}
					else{
						printf("Error\n");
					}

					double ang=90;
					if(wi==1)ang=-90;							
					cVec vr = (cellwidth/2.0)*v.rotate(ang,cVec(0.0,0.0,1.0)).unit();
					xc = xc+vr.x;
					yc = yc+vr.y;

					c0 = NullOutputProperty;
					if(wi==0 && li<nlayers && si<nsamples){
						c0 = c[si][li];
						if(c0 == NullInputConductivity){
							c0  = NullOutputProperty;							
						}
						else if(c0 <= 0.0){
							c0  = NullOutputProperty;
						}
						else{
							c0  = c[si][li];							
						}

						if(zc<NullBelowElevation){
							c0  = NullOutputProperty;							
						}

						double dc=ec-zc;
						if(dc>NullBelowDepth){
							c0  = NullOutputProperty;							
						}
					}

					fprintf(fp_data, "%8.1f %9.1f %7.1f %10.6f %4d %4d %4d\n", xc, yc, zc, c0, si, li, wi);
				}
			}		
		}
		fclose(fp_data);

		FILE* fp_hdr = fileopen(sgridhdrpath, "w");

		fprintf(fp_hdr, "GOCAD SGrid 1\n");
		fprintf(fp_hdr, "HEADER {\n");
		fprintf(fp_hdr, "name:%s\n", sgridname().c_str());
		fprintf(fp_hdr, "painted:true\n");
		fprintf(fp_hdr, "*painted*variable:Conductivity\n");
		fprintf(fp_hdr, "cage:false\n");
		fprintf(fp_hdr, "volume:true\n");
		fprintf(fp_hdr, "*volume*grid:false\n");
		fprintf(fp_hdr, "*volume*transparency_allowed:false\n");
		fprintf(fp_hdr, "*volume*points:false\n");
		fprintf(fp_hdr, "shaded_painted:false\n");
		fprintf(fp_hdr, "precise_painted:true\n");
		fprintf(fp_hdr, "*psections*grid:false\n");
		fprintf(fp_hdr, "*psections*solid:true\n");
		fprintf(fp_hdr, "dead_cells_faces:false\n");
		fprintf(fp_hdr, "}\n");

		fprintf(fp_hdr, "\n");
		fprintf(fp_hdr, "AXIS_N %d %d %d\n", nsamples+1, nlayers+1, 2);
		fprintf(fp_hdr, "PROP_ALIGNMENT CELLS\n");
		fprintf(fp_hdr, "ASCII_DATA_FILE %s\n", sgriddatafile.c_str());

		fprintf(fp_hdr, "\n");
		fprintf(fp_hdr, "PROPERTY 1 Conductivity\n");			
		fprintf(fp_hdr, "PROP_UNIT 1 S/m\n");
		fprintf(fp_hdr, "PROP_NO_DATA_VALUE 1 -999\n");			

		fprintf(fp_hdr, "\n");
		fprintf(fp_hdr, "END\n");
		fclose(fp_hdr);

	}

	void create_sgrid_prop_alignment_points(){

		static bool flipendian = !isbigendian();
		//must flip binary bytes to bigendian if not natively bigendian (MSBFIRST)
				
		std::string sgridhdrpath = sgriddir + sgridname() + ".sg";
		
		FILE* fp_asciidata = (FILE*)NULL;
		FILE* fp_points    = (FILE*)NULL;
		FILE* fp_property  = (FILE*)NULL;
				
		std::string asciidatapath = sgriddir + sgridname() + ".sg.data";
		std::string pointspath    = sgriddir + sgridname() + "_points@@";
		std::string proppath      = sgriddir + sgridname() + "_Conductivity@@";

		if (binary) {			
			fp_points = fileopen(pointspath, "w+b");
			fp_property = fileopen(proppath, "w+b");
		}
		else{						
			fp_asciidata = fileopen(asciidatapath, "w");
			fprintf(fp_asciidata, "*\n");
			fprintf(fp_asciidata, "*   X   Y   Z  Conductivity  I   J   K\n");
			fprintf(fp_asciidata, "*\n");			
		};

		for (int li = 0; li < nlayers; li++){
			for (int si = 0; si < nsamples; si++){
				double xc, yc, zc, ec, c0;
				cVec v;
				if (si>=0 && si<=nsamples){
					ec = e[si];
					xc = x[si];
					yc = y[si];
					zc = (z[si][li] + z[si][li+1])/2.0;
				}								
				else{
					printf("Error\n");
				}

				c0 = NullOutputProperty;
				if (li<nlayers && si<nsamples){
					c0 = c[si][li];
					if (c0 == NullInputConductivity){
						c0 = NullOutputProperty;						
					}
					else if (c0 <= 0.0){
						c0 = 1e-6;						
					}
					else{
						c0 = c[si][li];						
					}

					if (zc<NullBelowElevation){
						c0 = NullOutputProperty;						
					}

					double dc = ec - zc;
					if (dc>NullBelowDepth){
						c0 = NullOutputProperty;						
					}
				}

				if (binary) {
					float fxc = (float)xc;
					float fyc = (float)yc;
					float fzc = (float)zc;
					float fc0 = (float)c0;
					if (flipendian) {
						swap_endian(&fxc, 1);
						swap_endian(&fyc, 1);
						swap_endian(&fzc, 1);
						swap_endian(&fc0, 1);
					}

					fwrite(&fxc, sizeof(float), 1, fp_points);
					fwrite(&fyc, sizeof(float), 1, fp_points);
					fwrite(&fzc, sizeof(float), 1, fp_points);
					fwrite(&fc0, sizeof(float), 1, fp_property);
				}
				else{
					fprintf(fp_asciidata, "%8.1f %9.1f %7.1f %10.6f %4d %4d %4d\n", xc, yc, zc, c0, si, li, 0);
				}
			}
		}
		if (binary) {
			fclose(fp_points);
			fclose(fp_property);
		}
		else {
			fclose(fp_asciidata);
		}


		FILE* fp_hdr = fileopen(sgridhdrpath, "w");
		fprintf(fp_hdr, "GOCAD SGrid 1\n");
		fprintf(fp_hdr, "HEADER {\n");
		fprintf(fp_hdr, "name:%s\n", sgridname().c_str());
		fprintf(fp_hdr, "painted:true\n");
		fprintf(fp_hdr, "*painted*variable:Conductivity\n");
		fprintf(fp_hdr, "ascii:on\n");
		fprintf(fp_hdr, "double_precision_binary:off\n");
		fprintf(fp_hdr, "cage:false\n");
		fprintf(fp_hdr, "volume:true\n");
		fprintf(fp_hdr, "*volume*grid:false\n");
		fprintf(fp_hdr, "*volume*transparency_allowed:false\n");
		fprintf(fp_hdr, "*volume*points:false\n");
		fprintf(fp_hdr, "shaded_painted:false\n");
		fprintf(fp_hdr, "precise_painted:true\n");
		fprintf(fp_hdr, "*psections*grid:false\n");
		fprintf(fp_hdr, "*psections*solid:true\n");
		fprintf(fp_hdr, "dead_cells_faces:false\n");
		fprintf(fp_hdr, "}\n");

		fprintf(fp_hdr, "\n");
		fprintf(fp_hdr, "AXIS_N %d %d %d\n", nsamples, nlayers, 1);
		fprintf(fp_hdr, "PROP_ALIGNMENT POINTS\n");

		if (binary) {
			fprintf(fp_hdr, "POINTS_FILE %s\n", extractfilename(pointspath).c_str());
		}
		else {
			fprintf(fp_hdr, "ASCII_DATA_FILE %s\n", extractfilename(asciidatapath).c_str());
		}

		fprintf(fp_hdr, "\n");
		fprintf(fp_hdr, "PROPERTY 1 Conductivity\n");
		fprintf(fp_hdr, "PROP_UNIT 1 S/m\n");
		fprintf(fp_hdr, "PROP_NO_DATA_VALUE 1 -999\n");
		if (binary) {			
			fprintf(fp_hdr, "PROP_FILE 1 %s\n", extractfilename(proppath).c_str());
			fprintf(fp_hdr, "PROP_ESIZE 1 4\n");
			fprintf(fp_hdr, "PROP_ETYPE 1 IEEE\n");			
			fprintf(fp_hdr, "PROP_ALIGNMENT 1 POINTS\n");
			fprintf(fp_hdr, "PROP_FORMAT 1 RAW\n");
			fprintf(fp_hdr, "PROP_OFFSET 1 0\n");			
		}


		fprintf(fp_hdr, "\n");
		fprintf(fp_hdr, "END\n");
		fclose(fp_hdr);

		savexml();

	}

	std::string sgridname() {
		std::string name = sgridprefix + strprint("%d", linenumber) + sgridsuffix;
		return name;
	}

	std::string sgridhdrname() {
		std::string s = sgridname() + ".sg";
		return s;
	}

	std::string sgridhdrpath() {
		std::string s = sgriddir + sgridhdrname();
		return s;
	}

	std::string xmlname() {
		std::string s = sgridname() + ".xml";
		return s;
	}

	std::string xmlpath() {
		std::string s = sgriddir + xmlname();
		fixseparator(s);
		return s;
	}

	void savexml()
	{		
		cBlock b = mControl.findblock("XML");
		if (b.Entries.size() == 0) return;
		std::string crs = b.getstringvalue("CorodinateSystem");
		std::string cp  = b.getstringvalue("DataCachePrefix");
		if (cp[cp.size() - 1] != '/') cp += '/';
		std::string datacachename = cp + sgridhdrname();

		Document doc(xmlpath());
		std::string ver = "1.0";
		std::string enc = "UTF-8";
		std::string std = "yes";
		Declaration dec(ver, enc, std);
		doc.InsertEndChild(dec);

		Element l("Layer");		
		l.SetAttribute("layerType", "VolumeLayer");
		l.SetAttribute("version", "1");
		l.InsertEndChild(Element("DisplayName", sgridname()));
		l.InsertEndChild(Element("URL", sgridhdrname()));
		l.InsertEndChild(Element("DataFormat", "GOCAD SGrid"));
		l.InsertEndChild(Element("DataCacheName", datacachename));
		l.InsertEndChild(Element("CoordinateSystem", crs));
		doc.InsertEndChild(l);
		doc.SaveFile();
	}	

};

void save_dataset_xml(const std::string xmlpath, const std::string datasetname, const std::vector<std::string> names, const std::vector<std::string> urls)
{
	makedirectorydeep(extractfiledirectory(xmlpath));
	try
	{
		Element a, b;
		Document doc(xmlpath);

		Element dl("DatasetList");

		Element d("Dataset");
		d.SetAttribute("name", datasetname);		

		for (size_t i = 0; i < names.size(); i++) {
			Element l("Layer");
			l.SetAttribute("name", names[i]);
			l.SetAttribute("url", urls[i]);			
			d.InsertEndChild(l);
		}
		dl.InsertEndChild(d);
		doc.InsertEndChild(dl);
		doc.SaveFile();
	}
	catch (ticpp::Exception& ex)
	{
		std::cout << ex.what();
	}
}

int main(int argc, char** argv)
{
	if (argc >= 2){
		glog.logmsg("Executing %s %s\n", argv[0], argv[1]);
		glog.logmsg("Version %s Compiled at %s on %s\n", GAAEM_VERSION, __TIME__, __DATE__);
		glog.logmsg("Working directory %s\n", getcurrentdirectory().c_str());
	}
	else{
		glog.logmsg("Executing %s\n", argv[0]);
		glog.logmsg("Version %s Compiled at %s on %s\n", GAAEM_VERSION, __TIME__, __DATE__);
		glog.logmsg("Working directory %s\n", getcurrentdirectory().c_str());
		glog.logmsg("Error: Not enough input arguments\n");
		glog.logmsg("Usage: %s controlfilename\n",argv[0]);
		return 0;
	}
	
	std::vector<std::string> names;
	std::vector<std::string> urls; 
	std::string xmldir;

	cBlock control(argv[1]);	
	std::string infiles = control.getstringvalue("Input.DataFiles");
	cBlock xmlopt = control.findblock("XML");
	std::vector<std::string> filelist = cDirectoryAccess::getfilelist(infiles);	
	cStopWatch stopwatch;
	for (size_t i = 0; i < filelist.size(); i++){
		printf("Processing file %s   %3zu of %3zu\n", filelist[i].c_str(),i+1,filelist.size());
		cSGridCreator S(control);
		S.process(filelist[i]);		
		if (xmlopt.Entries.size() > 0) {
			names.push_back(S.sgridname());
			urls.push_back(S.xmlname());
			xmldir = extractfiledirectory(S.xmlpath());
		}		
	}
	
	if (xmlopt.Entries.size() > 0) {
		std::string datasetname = xmlopt.getstringvalue("DatasetName");
		std::string datasetxml  = xmldir + datasetname + ".xml";
		save_dataset_xml(datasetxml, datasetname, names, urls);
	}
	printf("Done ... \nElapsed time = %.3lf seconds\n", stopwatch.etimenow());	
	return 0;
}
