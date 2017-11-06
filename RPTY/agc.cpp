/*
    Alpha Gamma Counter
    Copyright (C) 2017  Mario Vretenar

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define VERSION "1.1"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <fstream>
#include <deque>
#include <thread>
#include <mutex>
#include "fpga.cpp"

using namespace std;

struct peak{
	long long unsigned time;
	int amp;
};

int alpha_thresh;
bool alpha_edge;	//0 - rising edge, 1 - falling edge
int gamma_thresh;
bool gamma_edge;
double alpha_mintime;unsigned alpha_mintime_uint;
double gamma_mintime;unsigned gamma_mintime_uint;
double interval;unsigned interval_uint;
unsigned delay_len;
bool delay_ch;
bool triggerisalpha;
unsigned step_alpha;
unsigned step_gamma;
unsigned alpha_po_avg;	//peak overlap parameter, 0<=par<=7
unsigned gamma_po_avg;

void _gen_conf(void)
{
	FILE *conffile;
	conffile=fopen("agc_conf.txt","w");
	fprintf(conffile,
		"Thresholds are minimum intensities required for trigger.\n"
		"alpha_thresh(-8192 - 8191):\t100\n"
		"alpha zero level (not needed by program, for reference):\t0\n"
		"alpha_edge(Rising (R) or Falling (F)):\tR\n"
		"gamma_thresh(-8192 - 8191):\t100\n"
		"gamma zero level (not needed by program, for reference):\t0\n"
		"gamma_edge(Rising (R) or Falling (F)):\tR\n"
		"Mintime is the minimum duration from threshold rising(falling) pass to falling(rising) pass for the peak to be registered. (in seconds)\n"
		"alpha_mintime(0 - 0.00052428):\t0.00001\n"
		"gamma_mintime(0 - 0.00052428):\t0.00001\n"
		"alpha avging for peak overlap detection (0-7):\t5\n"
		"gamma avging for peak overlap detection (0-7):\t5\n"
		"Add a time delay to the specified channel. (1==8e-9 s)\n"
		"delay_len(0 - 511):\t0\n"
		"delay_ch(A or B):\tA\n"
		"Observed interval after trigger event(0 - 34.3597)(in seconds):\t0.00001\n"
		"Trigger is alpha(y/n):\ty\n"
		"Time resolved alpha amplitude step:\t100000\n"
		"Time resolved gamma amplitude step:\t100000\n"
		);
	fclose(conffile);
}

void _load_conf(bool pf)
{
	ifstream t("agc_conf.txt");
	string conffile((istreambuf_iterator<char>(t)), istreambuf_iterator<char>());            //put the conf file into a string
	
	if (conffile.size()!=0){
		char tmpch; int z;
		size_t pos_alpha_thresh = conffile.find("alpha_thresh(-8192 - 8191):");
			if (pos_alpha_thresh != string::npos){
				pos_alpha_thresh+=27;
				sscanf(conffile.substr(pos_alpha_thresh).c_str(), "%d", &alpha_thresh);
				if(pf)printf("alpha_thresh=%d\n",alpha_thresh);
			}else {printf("Error in alpha_thresh. Delete file to regenerate from template.\n"); exit(0);}
		size_t pos_alpha_edge = conffile.find("alpha_edge(Rising (R) or Falling (F)):");
			if (pos_alpha_edge != string::npos){
				pos_alpha_edge+=38;
				z=0; do {sscanf(conffile.substr(pos_alpha_edge+z).c_str(), "%c", &tmpch); z++;}
				while (isspace(tmpch));
				if (tmpch=='R') alpha_edge=0;
				else if (tmpch=='F') alpha_edge=1;
				else {printf("Error in alpha_edge. Must be F or R!\n"); exit(0);}
				if(pf)printf("alpha_edge=%c\n",alpha_edge?'F':'R');
			}else {printf("Error in alpha_edge. Delete file to regenerate from template.\n"); exit(0);}
		size_t pos_gamma_thresh = conffile.find("gamma_thresh(-8192 - 8191):");
			if (pos_gamma_thresh != string::npos){
				pos_gamma_thresh+=27;
				sscanf(conffile.substr(pos_gamma_thresh).c_str(), "%d", &gamma_thresh);
				if(pf)printf("gamma_thresh=%d\n",gamma_thresh);
			}else {printf("Error in gamma_thresh. Delete file to regenerate from template.\n"); exit(0);}
		size_t pos_gamma_edge = conffile.find("gamma_edge(Rising (R) or Falling (F)):");
			if (pos_gamma_edge != string::npos){
				pos_gamma_edge+=38;
				z=0; do {sscanf(conffile.substr(pos_gamma_edge+z).c_str(), "%c", &tmpch); z++;}
				while (isspace(tmpch));
				if (tmpch=='R') gamma_edge=0;
				else if (tmpch=='F') gamma_edge=1;
				else {printf("Error in gamma_edge. Must be F or R!\n"); exit(0);}
				if(pf)printf("gamma_edge=%c\n",gamma_edge?'F':'R');
			}else {printf("Error in gamma_edge. Delete file to regenerate from template.\n"); exit(0);}
		size_t pos_alpha_mintime = conffile.find("alpha_mintime(0 - 0.00052428):");
			if (pos_alpha_mintime != string::npos){
				pos_alpha_mintime+=30;
				sscanf(conffile.substr(pos_alpha_mintime).c_str(), "%lf", &alpha_mintime);
				if(pf)printf("alpha_mintime=%lf\n",alpha_mintime);
			}else {printf("Error in alpha_mintime. Delete file to regenerate from template.\n"); exit(0);}
		size_t pos_gamma_mintime = conffile.find("gamma_mintime(0 - 0.00052428):");
			if (pos_gamma_mintime != string::npos){
				pos_gamma_mintime+=30;
				sscanf(conffile.substr(pos_gamma_mintime).c_str(), "%lf", &gamma_mintime);
				if(pf)printf("gamma_mintime=%lf\n",gamma_mintime);
			}else {printf("Error in gamma_mintime. Delete file to regenerate from template.\n"); exit(0);}
		size_t pos_delay_len = conffile.find("delay_len(0 - 511):");
			if (pos_delay_len != string::npos){
				pos_delay_len+=19;
				sscanf(conffile.substr(pos_delay_len).c_str(), "%u", &delay_len);
				if(pf)printf("delay_len=%u\n",delay_len);
			}else {printf("Error in delay_len. Delete file to regenerate from template.\n"); exit(0);}
		size_t pos_delay_ch = conffile.find("delay_ch(A or B):");
			if (pos_delay_ch != string::npos){
				pos_delay_ch+=17;
				z=0; do {sscanf(conffile.substr(pos_delay_ch+z).c_str(), "%c", &tmpch); z++;}
				while (isspace(tmpch));
				if (tmpch=='A') delay_ch=0;
				else if (tmpch=='B') delay_ch=1;
				else {printf("Error in delay_ch. Must be A or B!\n"); exit(0);}
				if(pf)printf("delay_ch=%c\n",delay_ch?'B':'A');
			}else {printf("Error in delay_ch. Delete file to regenerate from template.\n"); exit(0);}
		char tmp[100];
		size_t pos_interval = conffile.find("Observed interval after trigger event(0 - 34.3597)(in seconds):");
			if (pos_interval != string::npos){
				pos_interval+=63;
				sscanf(conffile.substr(pos_interval).c_str(), "%lf", &interval);
				if(pf)printf("interval=%lf\n",interval);
			}else {printf("Error in interval. Delete file to regenerate from template.\n"); exit(0);}
		size_t pos_triggerisalpha = conffile.find("Trigger is alpha(y/n):");
			if (pos_triggerisalpha != string::npos){
				pos_triggerisalpha+=22;
				sscanf(conffile.substr(pos_triggerisalpha).c_str(), "%s", tmp);
				triggerisalpha=((tmp[0]=='y')||(tmp[0]=='Y'))?true:false;
				if(pf)printf("triggerisalpha=%c\n",triggerisalpha?'y':'n');
			}else {printf("Error in triggerisalpha. Delete file to regenerate from template.\n"); exit(0);}
		size_t pos_step_alpha = conffile.find("Time resolved alpha amplitude step:");
			if (pos_step_alpha != string::npos){
				pos_step_alpha+=35;
				sscanf(conffile.substr(pos_step_alpha).c_str(), "%u", &step_alpha);
				if(pf)printf("step_alpha=%u\n",step_alpha);
			}else {printf("Error in step_alpha. Delete file to regenerate from template.\n"); exit(0);}
		size_t pos_step_gamma = conffile.find("Time resolved gamma amplitude step:");
			if (pos_step_gamma != string::npos){
				pos_step_gamma+=35;
				sscanf(conffile.substr(pos_step_gamma).c_str(), "%u", &step_gamma);
				if(pf)printf("step_gamma=%u\n",step_gamma);
			}else {printf("Error in step_gamma. Delete file to regenerate from template.\n"); exit(0);}	
		size_t pos_alpha_po_avg = conffile.find("alpha avging for peak overlap detection (0-7):");
			if (pos_alpha_po_avg != string::npos){
				pos_alpha_po_avg+=46;
				sscanf(conffile.substr(pos_alpha_po_avg).c_str(), "%u", &alpha_po_avg);
				if(pf)printf("alpha_po_avg=%u\n",alpha_po_avg);
			}else {printf("Error in alpha_po_avg. Delete file to regenerate from template.\n"); exit(0);}
		size_t pos_gamma_po_avg = conffile.find("gamma avging for peak overlap detection (0-7):");
			if (pos_gamma_po_avg != string::npos){
				pos_gamma_po_avg+=46;
				sscanf(conffile.substr(pos_gamma_po_avg).c_str(), "%u", &gamma_po_avg);
				if(pf)printf("gamma_po_avg=%u\n",gamma_po_avg);
			}else {printf("Error in gamma_po_avg. Delete file to regenerate from template.\n"); exit(0);}
			
		if(pf)printf("All loaded, no errors (I did not check for boundaries, you better had chosen them properly)!.\n");
	}
	else{
		printf ("No agc_conf.txt found. Generating file from template. Modify the file and rerun the program.\n");
		_gen_conf();
		exit(0);
	}
	t.close();
}

bool _match_confs()	//compare configs
{
	ifstream t0("agc_conf.txt");
	string conffile0((istreambuf_iterator<char>(t0)), istreambuf_iterator<char>());
	ifstream t1("measurements/agc_conf.txt");
	string conffile2((istreambuf_iterator<char>(t1)), istreambuf_iterator<char>());
	if (conffile2.empty()){
		system ("cp agc_conf.txt measurements/agc_conf.txt");
	}
	else if (conffile0.compare(conffile2) != 0) return true;
	return false;
}

mutex endack_mx;
bool endack=false;
void term_fun(void){
	char command;
	for (;;){
		scanf("%c",&command);
		if (command=='e'){
			endack_mx.lock();
			endack=true;
			endack_mx.unlock();
			return;
		}
	}
}

int main(int argc,char *argv[]){
	bool pf=true;
	if (argc==2) pf=false;
	if(pf) printf("Alpha-gamma counter program, version: %s\n",VERSION);
	if (argc>2){printf ("The program takes either no arguments or a single time acqusition parameter (integer - time in seconds) for background acquisition.\n");return 0;}
	if(pf)printf ("You may also start this program in background with a fixed acquisition duration by starting it with an time argument (integer - time in seconds). (like \"./agc.out 3600 &\")\n");
	if(pf)printf ("Note that existing .dat files are read and new counts are added to existing ones. If the settings change (such as energy boundaries) these files should be removed"
	              ", else the program may crash (because wrong file lenghts etc.).\n\n");
	_load_conf(pf);
	alpha_mintime_uint=(unsigned)(alpha_mintime*125000000);
	gamma_mintime_uint=(unsigned)(gamma_mintime*125000000);
	interval_uint=(unsigned)(interval*125000000);

	//check if red_pitaya_agcv_VERSION.bit exists
	FILE* binFILE;
	string fnamecomm="red_pitaya_agc_v";
	fnamecomm += VERSION;
	fnamecomm += ".bit";
	binFILE = fopen(fnamecomm.c_str(),"rb");
	if (binFILE!=NULL) fclose(binFILE);
	else {printf ("FILE %s NOT FOUND. ABORTING.\n",fnamecomm.c_str());return 0;}
	fnamecomm.insert(0,"cat ");
	fnamecomm+= " > /dev/xdevcfg";
	system (fnamecomm.c_str());
	
	//make measurements folder if missing, and check if existing configuration inside it matches the config in the working directory
	system ("mkdir measurements -p");
	if (_match_confs()) {printf ("Configuration in working directory does not match the one in measurements folder. "
			"You should rename the measurements folder to prevent appending new data with different configuration. Aborting.\n",fnamecomm.c_str());return 0;}
	
	int ENmax_alpha;
	if (!alpha_edge){	//rising edge
		ENmax_alpha=8191-alpha_thresh+1;	//num of elements in the array
	}else{
		ENmax_alpha=-(-8192-alpha_thresh)+1;	//num of elements in the array
	}
	unsigned alpha_binN = ENmax_alpha/step_alpha+1;
	if(pf)printf("\nalpha_binN=%u\n",alpha_binN);
	int ENmax_gamma;
	if (!gamma_edge){
		ENmax_gamma=8191-gamma_thresh+1;	//num of elements in the array
	}else{
		ENmax_gamma=-(-8192-gamma_thresh)+1;	//num of elements in the array
	}
	unsigned gamma_binN = ENmax_gamma/step_gamma+1;
	if(pf)printf("gamma_binN=%u\n\n",gamma_binN);
	
	long unsigned memreq;
	memreq=ENmax_alpha+ENmax_gamma+alpha_binN*gamma_binN*interval_uint;
	memreq*=sizeof(unsigned);
	printf("Total memory required (both in RAM and SD): %.4lf MB. MAKE SURE IT IS AVAILABLE BEFORE PROCEEDING.\n",(double)memreq/1024/1024);
	if(pf){	printf("Press any key to continue...\n");
		scanf("%*c");
	}
	
	if (AGC_init()) return -1;		//fpga init
	AGC_setup(alpha_thresh,gamma_thresh,alpha_edge,gamma_edge,alpha_mintime_uint,gamma_mintime_uint,delay_len,delay_ch);
	AGC_set_overlap(alpha_po_avg, gamma_po_avg);
	
	if(pf){
		thread term_thread (term_fun);			//ending by button
		term_thread.detach();			
	}
       
       
        FILE* ifile;
		//####generate alpha energy array
	ifile = fopen("measurements/alpha.dat","rb");
	unsigned *alpha_array = new unsigned[ENmax_alpha];
	if (ifile!=NULL) {
		fread (alpha_array,sizeof(unsigned),ENmax_alpha,ifile);	// read existing file
		fclose(ifile);
	}
	else for (int i=0;i!=ENmax_alpha;i++)alpha_array[i]=0;	// else fill with 0
	
		//####generate gamma energy array
	ifile = fopen("measurements/gamma.dat","rb");
	unsigned *gamma_array = new unsigned[ENmax_gamma];
	if (ifile!=NULL) {
		fread (gamma_array,sizeof(unsigned),ENmax_gamma,ifile);	// read existing file
		fclose(ifile);
	}
	else for (int i=0;i!=ENmax_gamma;i++)gamma_array[i]=0;	// else fill with 0
	
		//####generate time arrays
	
	ifile = fopen("measurements/time.dat","rb");
	unsigned*** bins = new unsigned**[alpha_binN];
	for (int i=0; i!=alpha_binN;i++) {
		bins[i] = new unsigned*[gamma_binN];
		for (int j=0; j!=gamma_binN;j++){
			bins[i][j] = new unsigned[interval_uint];
			if (ifile!=NULL) fread (bins[i][j],sizeof(unsigned),interval_uint,ifile);	// read existing file
			else for (int k=0;k!=interval_uint;k++)bins[i][j][k]=0;
		}
	}
	if (ifile!=NULL) fclose(ifile);	

	long long unsigned N_alpha=0;
        long long unsigned N_gamma=0;		//on PC long long int is 8byte, long int is 8byte, int is 4byte
        long long unsigned counter=0;		//on ARM long long int is 8byte, long int is 4byte, int is 4byte	//TODO maybe use uint32_t 
        long long unsigned event_ts;
        
        deque <peak> active_trig;
        bool isalpha;
	int amplitude;
	unsigned int cntr_t0;
	unsigned int cntr_t1;
	
	AGC_reset_fifo(); 
	for(int i=0;;i++){
		if (!AGC_get_sample(&isalpha,&amplitude,&cntr_t0,&cntr_t1)){
			if (isalpha){
				N_alpha++;
				alpha_array[abs(amplitude-alpha_thresh)]++;	
			}
			else{
				N_gamma++;
				gamma_array[abs(amplitude-gamma_thresh)]++;
			}
			
			event_ts=counter+cntr_t0-cntr_t1;
			if (triggerisalpha == isalpha){
				active_trig.emplace_back();
				active_trig.back().time=event_ts;
				active_trig.back().amp=amplitude;
			}else{	//the other is trigger
				for (int j=0;j!=active_trig.size();j++){
					if (event_ts>active_trig[j].time+interval_uint){
						active_trig.pop_front();
						j--;
					}else{
						unsigned a,b;
						if (active_trig[j].time<=event_ts) {
							if (triggerisalpha) a=abs(active_trig[j].amp-alpha_thresh)/step_alpha;
							else a=abs(amplitude-alpha_thresh)/step_alpha;
							if (triggerisalpha) b=abs(amplitude-gamma_thresh)/step_gamma;
							else b=abs(active_trig[j].amp-gamma_thresh)/step_gamma;
							bins[a][b][event_ts-active_trig[j].time]++;
						}
					}
				}	
			}
			counter+=cntr_t0;
		}
	

		if (i/1000000){
			if(pf){
				endack_mx.lock();
				if (endack) break;
				endack_mx.unlock();
			}
			else if (counter/125000000>=atoi(argv[1])) break;
			
			if(pf)printf ("\033[2JPress 'e' to stop acquistion.\nN_alpha=%llu\nN_gamma=%llu\nelapsed time=%llu s\n"
			              "RPTY lost peaks:%u(max in queue %u/250)\n"
			              "alpha_npeaks2=%u\nalpha_npeaks3=%u\nalpha_npeaks4+=%u\n"
			              "gamma_npeaks2=%u\ngamma_npeaks3=%u\ngamma_npeaks4+=%u\n"
			              ,N_alpha,N_gamma,counter/125000000,AGC_get_num_lost(),AGC_get_max_in_queue()
			              ,AGC_get_overlap(0),AGC_get_overlap(1),AGC_get_overlap(2)
			              ,AGC_get_overlap(3),AGC_get_overlap(4),AGC_get_overlap(5) );
			i=0;
		}
		
	}
	
	unsigned oploss[6];
	oploss[0]=AGC_get_overlap(0); oploss[1]=AGC_get_overlap(1); oploss[2]=AGC_get_overlap(2); 
	oploss[3]=AGC_get_overlap(3); oploss[4]=AGC_get_overlap(4); oploss[5]=AGC_get_overlap(5); 
	if(pf)printf ("\033[2JAcquistion ended.\nN_alpha=%llu\nN_gamma=%llu\nelapsed time=%llu s\n"
	              "RPTY lost peaks:%u(max in queue %u/250)\n"
	              "alpha_npeaks2=%u\nalpha_npeaks3=%u\nalpha_npeaks4+=%u\n"
	              "gamma_npeaks2=%u\ngamma_npeaks3=%u\ngamma_npeaks4+=%u\n"
	              ,N_alpha,N_gamma,counter/125000000,AGC_get_num_lost(),AGC_get_max_in_queue()
	              ,oploss[0],oploss[1],oploss[2],oploss[3],oploss[4],oploss[5] );
	
	FILE* ofile;
	
	if(pf)printf("Saving alpha...");
	ofile=fopen("measurements/alpha.dat","wb");
	fwrite (alpha_array,sizeof(unsigned),ENmax_alpha,ofile);
	fclose(ofile);
	if(pf)printf("done! format is \'%%uint32\' starting from threshold(=0). One line is one channel.\n");
	delete[] alpha_array;

	if(pf)printf("Saving gamma...");
	ofile=fopen("measurements/gamma.dat","wb");
	fwrite (gamma_array,sizeof(unsigned),ENmax_gamma,ofile);
	fclose(ofile);
	if(pf)printf("done! format is \'%%uint32\' starting from threshold(=0). One line is one channel.\n");
	delete[] gamma_array;

	
	if(pf)printf("Saving time...");
	ofile = fopen("measurements/time.dat","wb");
	for (int i=0; i!=alpha_binN;i++) {
		for (int j=0; j!=gamma_binN;j++){
			fwrite (bins[i][j],sizeof(unsigned),interval_uint,ofile);
			delete[] bins[i][j];
		}
		delete[] bins[i];
	}
	delete[] bins;
	fclose(ofile);
	if(pf)printf("done! format is \'%%uint32\' and is a 3D matrix of size %d:%d:%d.\n",alpha_binN,gamma_binN,interval_uint);
	
	if(pf)printf("Saving duration...");
	ofile=fopen("measurements/duration.txt","a");
	fprintf(ofile,"+%llu seconds\nN_alpha:%llu\nN_gamma:%llu\noploss:%u/%u/%u/%u/%u/%u\n\n",counter/125000000
	        ,N_alpha,N_gamma,oploss[0],oploss[1],oploss[2],oploss[3],oploss[4],oploss[5] );
	fclose(ofile);
	
	AGC_exit();
}
