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

//compilation: g++ -pthread -std=c++11 agc.cpp -o agc

#include <cstdio>
#include <cstdlib>
#include <string>
#include <fstream>
#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <array>
#include "fpga.cpp"

using namespace std;

struct ackstr{
	string name;
	int alpha_min;
	int alpha_max;
	int gamma_min;
	int gamma_max;
	unsigned *time_array;
};

struct peak{
	long long unsigned time;
	int amp;
};

int alpha_thresh;
int gamma_thresh;
double alpha_mintime;unsigned alpha_mintime_uint;
double gamma_mintime;unsigned gamma_mintime_uint;
bool count_A_alpha;
bool count_A_gamma;
double interval;unsigned interval_uint;
unsigned alpha_delay;
bool triggerisalpha;
vector <ackstr> bins;


void _gen_conf(void)
{
	FILE *conffile;
	conffile=fopen("agc_conf.txt","w");
	fprintf(conffile,
		"Thresholds are minimum intensities required for trigger. For negative thresholds the peak is assumed to be inverted.\n"
		"alpha_thresh(-8192 - 8191):\t8191\n"
		"gamma_thresh(-8192 - 8191):\t8191\n"
		"Mintime is the minimum duration from threshold rising pass to falling pass for the peak to be registered. (in seconds)\n"
		"alpha_mintime(0 - 34.3597):\t10\n"
		"gamma_mintime(0 - 34.3597):\t10\n"
		"Delay can be used to add a time delay on channel A. (1==1e-8 s)\n"
		"alpha_delay(0 - 255):\t0\n"
		"\n"
		"Count the number of events per alpha amplitude (y/n):\tn\n"
		"Count the number of events per gamma amplitude (y/n):\tn\n"
		"Observed interval after trigger event(0 - 34.3597):\t1\n"
		"Trigger is alpha(y/n):\ty\n"
		"Acquisition bins(each line is one bin): bin_name(has to be Bin0, Bin1 etc. exactly, case sensitive) alpha_Amin alpha_Amax gamma_Amin gamma_Amax\n"
		"Bin0 0 8191 0 8191\n"
		);
	fclose(conffile);
}

void _load_conf(bool pf)
{
	ifstream t("agc_conf.txt");
	string conffile((istreambuf_iterator<char>(t)), istreambuf_iterator<char>());            //put the conf file into a string
	
	if (conffile.size()!=0){
		size_t pos_alpha_thresh = conffile.find("alpha_thresh(-8192 - 8191):");
		if (pos_alpha_thresh != string::npos){
			pos_alpha_thresh+=27;
			sscanf(conffile.substr(pos_alpha_thresh).c_str(), "%d", &alpha_thresh);
			if(pf)printf("alpha_thresh=%d\n",alpha_thresh);
		}else {printf("Error in alpha_thresh. Delete file to regenerate from template.\n"); exit(0);}
		size_t pos_gamma_thresh = conffile.find("gamma_thresh(-8192 - 8191):");
		if (pos_gamma_thresh != string::npos){
			pos_gamma_thresh+=27;
			sscanf(conffile.substr(pos_gamma_thresh).c_str(), "%d", &gamma_thresh);
			if(pf)printf("gamma_thresh=%d\n",gamma_thresh);
		}else {printf("Error in gamma_thresh. Delete file to regenerate from template.\n"); exit(0);}
		size_t pos_alpha_mintime = conffile.find("alpha_mintime(0 - 34.3597):");
		if (pos_alpha_mintime != string::npos){
			pos_alpha_mintime+=27;
			sscanf(conffile.substr(pos_alpha_mintime).c_str(), "%lf", &alpha_mintime);
			if(pf)printf("alpha_mintime=%lf\n",alpha_mintime);
		}else {printf("Error in alpha_mintime. Delete file to regenerate from template.\n"); exit(0);}
		size_t pos_gamma_mintime = conffile.find("gamma_mintime(0 - 34.3597):");
		if (pos_gamma_mintime != string::npos){
			pos_gamma_mintime+=27;
			sscanf(conffile.substr(pos_gamma_mintime).c_str(), "%lf", &gamma_mintime);
			if(pf)printf("gamma_mintime=%lf\n",gamma_mintime);
		}else {printf("Error in gamma_mintime. Delete file to regenerate from template.\n"); exit(0);}
		size_t pos_alpha_delay = conffile.find("alpha_delay(0 - 255):");
		if (pos_alpha_delay != string::npos){
			pos_alpha_delay+=21;
			sscanf(conffile.substr(pos_alpha_delay).c_str(), "%u", &alpha_delay);
			if(pf)printf("alpha_delay=%u\n",alpha_delay);
		}else {printf("Error in alpha_delay. Delete file to regenerate from template.\n"); exit(0);}
		char tmp[100];
		size_t pos_count_A_alpha = conffile.find("Count the number of events per alpha amplitude (y/n):");
		if (pos_count_A_alpha != string::npos){
			pos_count_A_alpha+=53;
			sscanf(conffile.substr(pos_count_A_alpha).c_str(), "%s", tmp);
			count_A_alpha=((tmp[0]=='y')||(tmp[0]=='Y'))?true:false;
			if(pf)printf("count_A_alpha=%c\n",count_A_alpha?'y':'n');
		}else {printf("Error in count_A_alpha. Delete file to regenerate from template.\n"); exit(0);}
		
		size_t pos_count_A_gamma = conffile.find("Count the number of events per gamma amplitude (y/n):");
		if (pos_count_A_gamma != string::npos){
			pos_count_A_gamma+=53;
			sscanf(conffile.substr(pos_count_A_gamma).c_str(), "%s", tmp);
			count_A_gamma=((tmp[0]=='y')||(tmp[0]=='Y'))?true:false;
			if(pf)printf("count_A_gamma=%c\n",count_A_gamma?'y':'n');
		}else {printf("Error in count_A_gamma. Delete file to regenerate from template.\n"); exit(0);}
		size_t pos_interval = conffile.find("Observed interval after trigger event(0 - 34.3597):");
		if (pos_interval != string::npos){
			pos_interval+=51;
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
		int tmpint[4];
		size_t pos_bins; string binname;
		size_t pos_bins_0 = conffile.find("Acquisition bins(each line is one bin): bin_name(has to be Bin0, Bin1 etc. exactly, case sensitive) alpha_Amin alpha_Amax gamma_Amin gamma_Amax");
		if (pos_bins_0 == string::npos) {printf("Error in bins. Delete file to regenerate from template.\n"); exit(0);}
		pos_bins_0+=143;
		for (int i=0;;i++){
			binname="Bin"+to_string(i);
			pos_bins = conffile.substr(pos_bins_0).find(binname.c_str());
			if (pos_bins != string::npos){
				pos_bins+=binname.size();
				sscanf(conffile.substr(pos_bins_0).substr(pos_bins).c_str(), "%d%d%d%d",&tmpint[0],&tmpint[1],&tmpint[2],&tmpint[3]);
				bins.emplace_back();
				bins.back().name=binname;
				bins.back().alpha_min=tmpint[0];
				bins.back().alpha_max=tmpint[1];
				bins.back().gamma_min=tmpint[2];
				bins.back().gamma_max=tmpint[3];
				if(pf)printf("%s amin=%d amax=%d gmin=%d gmax=%d\n",bins.back().name.c_str(),bins.back().alpha_min,bins.back().alpha_max,bins.back().gamma_min,bins.back().gamma_max);
			}else if (i==0){printf("Error in %s (not found). Delete file to regenerate from template.\n",binname.c_str()); exit(0);}
			else break;
		}
		if(pf)printf("All loaded, no errors (i did not check for boundaries, you had better speced them right)!.\n");
	}
	else{
		printf ("No agc_conf.txt found. Generating file from template. Modify the file and rerun the program.\n");
		_gen_conf();
		exit(0);
	}
	t.close();
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
void end_fun(double time){
	string command="sleep "+to_string(time);
	system (command.c_str());
	endack_mx.lock();
	endack=true;
	endack_mx.unlock();
	return;
}

int main(int argc,char *argv[]){
	if (argc>2){printf ("The program takes either no arguments or a single time acqusition parameter for background acquisition.\n");return 0;}
	bool pf=true;
	if (argc==2) pf=false;
	
	if(pf)printf ("You may also start this program in background with a fixed acquisition duration by starting it with an time argument. (like \"./agc.out 3600 &\")\n");
	if(pf)printf ("Note that existing .dat files are read and new counts are added to existing ones. If the settings change (such as energy boundaries) these files should be removed, else the program may crash (because wrong file lenghts etc.).\n\n");
	_load_conf(pf);
	alpha_mintime_uint=(unsigned)(alpha_mintime*125000000);
	gamma_mintime_uint=(unsigned)(gamma_mintime*125000000);
	interval_uint=(unsigned)(interval*125000000);
	
	system ("cat red_pitaya.bin > /dev/xdevcfg");
	if(pf){	printf("Press any key to continue...\n");
		scanf("%*c");
	}
	
	if (AGC_init()) return -1;		//fpga init
	AGC_setup(alpha_thresh,gamma_thresh,alpha_mintime_uint,gamma_mintime_uint,alpha_delay);	//TODO fix signs
	

	if(pf){
		thread term_thread (term_fun);			//endinf by button
		term_thread.detach();
	}else{
		thread end_thread (end_fun,atof(argv[1]));	//ending by timer
        	end_thread.detach();
	}
       
        FILE* ifile;
		//####generate alpha energy array
	int alpha_array_offset, ENmax_alpha;
	if (alpha_thresh>=0){
		alpha_array_offset=alpha_thresh;	//we must subtract array index by offset
		ENmax_alpha=8191-alpha_thresh+1;	//num of elements in the array
	}else{
		alpha_array_offset=-8192;		//we must subtract array index by offset
		ENmax_alpha=-(-8192-alpha_thresh)+1;	//num of elements in the array
	}
	if (!count_A_alpha) ENmax_alpha=0;
	else ifile = fopen("alpha.dat","rb");
	unsigned *alpha_array = new unsigned[ENmax_alpha];
	if (ifile!=NULL) {
		fread (alpha_array,sizeof(unsigned),ENmax_alpha,ifile);	// read existing file
		fclose(ifile);
	}
	else for (int i=0;i!=ENmax_alpha;i++)alpha_array[i]=0;	// else fill with 0
	
		//####generate gamma energy array
	int gamma_array_offset, ENmax_gamma;
	if (gamma_thresh>=0){
		gamma_array_offset=gamma_thresh;	//we must subtract array index by offset
		ENmax_gamma=8191-gamma_thresh+1;	//num of elements in the array
	}else{
		gamma_array_offset=-8192;		//we must subtract array index by offset
		ENmax_gamma=-(-8192-gamma_thresh)+1;	//num of elements in the array
	}
	if (!count_A_gamma) ENmax_gamma=0;
	else ifile = fopen("gamma.dat","rb");
	unsigned *gamma_array = new unsigned[ENmax_gamma];
	if (ifile!=NULL) {
		fread (gamma_array,sizeof(unsigned),ENmax_gamma,ifile);	// read existing file
		fclose(ifile);
	}
	else for (int i=0;i!=ENmax_gamma;i++)gamma_array[i]=0;	// else fill with 0
	
		//####generate time arrays
	for (int i=0; i!=bins.size();i++){
		bins[i].time_array = new unsigned[interval_uint];
		string fname = bins[i].name+".dat";
		ifile = fopen(fname.c_str(),"rb");
		if (ifile!=NULL) {
			fread (bins[i].time_array,sizeof(unsigned),interval_uint,ifile);	// read existing file
			fclose(ifile);
		}
		else for (int j=0;j!=interval_uint;j++)bins[i].time_array[j]=0;	// else fill with 0
	}
	
	long long unsigned N_alpha=0;
        long long unsigned N_gamma=0;		//on PC long long int is 8byte, long int is 8byte, int is 4byte
        long long unsigned counter=0;		//on ARM long long int is 8byte, long int is 4byte, int is 4byte
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
				if (count_A_alpha){
					alpha_array[amplitude-alpha_array_offset]++;
				}
			}
			else{
				N_gamma++;
				if (count_A_gamma){
					gamma_array[amplitude-gamma_array_offset]++;
				}
				
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
						if (active_trig[j].time<=event_ts) for (int k=0; k!=bins.size();k++){
							if ( (active_trig[j].amp >= bins[k].alpha_min
							   && active_trig[j].amp <= bins[k].alpha_max
							   && amplitude >= bins[k].gamma_min
							   && amplitude <= bins[k].gamma_max )&&(triggerisalpha) )
								bins[k].time_array[event_ts-active_trig[j].time]++;
							else if (active_trig[j].amp >= bins[k].gamma_min
							      && active_trig[j].amp <= bins[k].gamma_max
							      && amplitude >= bins[k].alpha_min
							      && amplitude <= bins[k].alpha_max )
							      	bins[k].time_array[event_ts-active_trig[j].time]++;
						}
					}
				}	
			}
			counter+=cntr_t0;
		}
	

		if (i/1000000){
			endack_mx.lock();
			if (endack) break;
			endack_mx.unlock();
			//TODO put in separate thread to speed up
			if(pf)printf ("\033[2JPress 'e' to stop acquistion.\nN_alpha=%llu\nN_gamma=%llu\nelapsed time=%llu s\nRPTY lost peaks:%u(max in queue %u/200)\n",N_alpha,N_gamma,counter/125000000,AGC_get_num_lost(),AGC_get_max_in_queue());
			i=0;
		}
		
	}
	
	if(pf)printf ("\033[2JAcquistion ended.\nN_alpha=%llu\nN_gamma=%llu\nelapsed time=%llu s\nRPTY lost peaks:%u(max in queue %u/200)\n",N_alpha,N_gamma,counter/125000000,AGC_get_num_lost(),AGC_get_max_in_queue());
	
	
	if (count_A_alpha){	//save data
		if(pf)printf("Saving alpha...");
		FILE* ofile;
		ofile=fopen("alpha.dat","wb");
		fwrite (alpha_array,sizeof(unsigned),ENmax_alpha,ofile);
		fclose(ofile);
		if(pf)printf("done!\n");
	}
	delete[] alpha_array;
	
	if (count_A_gamma){
		if(pf)printf("Saving gamma...");
		FILE* ofile;
		ofile=fopen("gamma.dat","wb");
		fwrite (gamma_array,sizeof(unsigned),ENmax_gamma,ofile);
		fclose(ofile);
		if(pf)printf("done!\n");
	}
	delete[] gamma_array;
	
	for (int i=0; i!=bins.size();i++){
		if(pf)printf("Saving time %s...",bins[i].name.c_str());
		FILE* ofile;
		string fname = bins[i].name+".dat";
		ofile=fopen(fname.c_str(),"wb");
		fwrite (bins[i].time_array,sizeof(unsigned),interval_uint,ofile);
		fclose(ofile);
		if(pf)printf("done!\n");
		delete[] bins[i].time_array;
	}
	
	
	AGC_exit();
}
