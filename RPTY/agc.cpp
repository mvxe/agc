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
#include <time.h>
#include "fpga.cpp"

using namespace std;

struct peak{
	long long unsigned time;
	int ratio[2];
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
int ratio0_lo[2];	//100*ln(t1/amp)
int ratio0_up[2];	//[0]=alpha, [1]=gamma
int ratio1_lo[2];	//100*ln(t1/t2)
int ratio1_up[2];
unsigned step_alpha;
unsigned step_gamma;


void _gen_conf(void)
{
	FILE *conffile;
	conffile=fopen("agc_conf.txt","w");
	fprintf(conffile,
		"Thresholds are minimum intensities required for trigger.\n"
		"alpha_thresh(-8192 - 8191):\t8191\n"
		"alpha_edge(Rising (R) or Falling (F)):\tR\n"
		"gamma_thresh(-8192 - 8191):\t8191\n"
		"gamma_edge(Rising (R) or Falling (F)):\tR\n"
		"Mintime is the minimum duration from threshold rising(falling) pass to falling(rising) pass for the peak to be registered. (in seconds)\n"
		"alpha_mintime(0 - 34.3597):\t10\n"
		"gamma_mintime(0 - 34.3597):\t10\n"
		"Add a time delay to the specified channel. (1==1e-8 s)\n"
		"delay_len(0 - 511):\t0\n"
		"delay_ch(A or B):\tA\n"
		"\n"
		"Observed interval after trigger event(0 - 34.3597)(in seconds):\t0.00001\n"
		"Trigger is alpha(y/n):\ty\n"
		"Lower t1/amp alpha ratio (100*ln(rat)):\t-1000\n"
		"Upper t1/amp alpha ratio (100*ln(rat)):\t1000\n"
		"Lower t1/t2 alpha ratio (100*ln(rat)):\t-1000\n"
		"Upper t1/t2 alpha ratio (100*ln(rat)):\t1000\n"
		"Lower t1/amp gamma ratio (100*ln(rat)):\t-1000\n"
		"Upper t1/amp gamma ratio (100*ln(rat)):\t1000\n"
		"Lower t1/t2 gamma ratio (100*ln(rat)):\t-1000\n"
		"Upper t1/t2 gamma ratio (100*ln(rat)):\t1000\n"
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
		char tmpch;
		size_t pos_alpha_thresh = conffile.find("alpha_thresh(-8192 - 8191):");
			if (pos_alpha_thresh != string::npos){
				pos_alpha_thresh+=27;
				sscanf(conffile.substr(pos_alpha_thresh).c_str(), "%d", &alpha_thresh);
				if(pf)printf("alpha_thresh=%d\n",alpha_thresh);
			}else {printf("Error in alpha_thresh. Delete file to regenerate from template.\n"); exit(0);}
		size_t pos_alpha_edge = conffile.find("alpha_edge(Rising (R) or Falling (F)):");
			if (pos_alpha_edge != string::npos){
				pos_alpha_edge+=38;
				do sscanf(conffile.substr(pos_alpha_edge).c_str(), "%c", &tmpch);
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
				do sscanf(conffile.substr(pos_gamma_edge).c_str(), "%c", &tmpch);
				while (isspace(tmpch));
				if (tmpch=='R') gamma_edge=0;
				else if (tmpch=='F') gamma_edge=1;
				else {printf("Error in gamma_edge. Must be F or R!\n"); exit(0);}
				if(pf)printf("gamma_edge=%c\n",gamma_edge?'F':'R');
			}else {printf("Error in gamma_edge. Delete file to regenerate from template.\n"); exit(0);}
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
		size_t pos_delay_len = conffile.find("delay_len(0 - 511):");
			if (pos_delay_len != string::npos){
				pos_delay_len+=19;
				sscanf(conffile.substr(pos_delay_len).c_str(), "%u", &delay_len);
				if(pf)printf("delay_len=%u\n",delay_len);
			}else {printf("Error in delay_len. Delete file to regenerate from template.\n"); exit(0);}
		size_t pos_delay_ch = conffile.find("delay_ch(A or B):");
			if (pos_delay_ch != string::npos){
				pos_delay_ch+=17;
				do sscanf(conffile.substr(pos_delay_ch).c_str(), "%c", &tmpch);
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
		size_t pos_ratio0_loa= conffile.find("Lower t1/amp alpha ratio (100*ln(rat)):");
			if (pos_ratio0_loa != string::npos){
				pos_ratio0_loa+=39;
				sscanf(conffile.substr(pos_ratio0_loa).c_str(), "%d", &ratio0_lo[0]);
				if(pf)printf("alpha ratio0_lo (100*ln(rat)) =%d\n",ratio0_lo[0]);
			}else {printf("Error in alpha ratio0_lo. Delete file to regenerate from template.\n"); exit(0);}
		size_t pos_ratio0_upa= conffile.find("Upper t1/amp alpha ratio (100*ln(rat)):");
			if (pos_ratio0_upa != string::npos){
				pos_ratio0_upa+=39;
				sscanf(conffile.substr(pos_ratio0_upa).c_str(), "%d", &ratio0_up[0]);
				if(ratio0_up[0]<=ratio0_lo[0]) {printf("Error: ratio0_up<=ratio0_lo.\n"); exit(0);}
				if(pf)printf("alpha ratio0_up (100*ln(rat)) =%d\n",ratio0_up[0]);
			}else {printf("Error in alpha ratio0_up. Delete file to regenerate from template.\n"); exit(0);}
		size_t pos_ratio1_loa= conffile.find("Lower t1/t2 alpha ratio (100*ln(rat)):");
			if (pos_ratio1_loa != string::npos){
				pos_ratio1_loa+=38;
				sscanf(conffile.substr(pos_ratio1_loa).c_str(), "%d", &ratio1_lo[0]);
				if(pf)printf("alpha ratio1_lo (100*ln(rat)) =%d\n",ratio1_lo[0]);
			}else {printf("Error in alpha ratio1_lo. Delete file to regenerate from template.\n"); exit(0);}
		size_t pos_ratio1_upa= conffile.find("Upper t1/t2 alpha ratio (100*ln(rat)):");
			if (pos_ratio1_upa != string::npos){
				pos_ratio1_upa+=38;
				sscanf(conffile.substr(pos_ratio1_upa).c_str(), "%d", &ratio1_up[0]);
				if(ratio1_up[0]<=ratio1_lo[0]) {printf("Error: alpha ratio1_up<=ratio1_lo.\n"); exit(0);}
				if(pf)printf("alpha ratio1_up (100*ln(rat)) =%d\n",ratio1_up[0]);
			}else {printf("Error in alpha ratio1_up. Delete file to regenerate from template.\n"); exit(0);}
		size_t pos_ratio0_log= conffile.find("Lower t1/amp gamma ratio (100*ln(rat)):");
			if (pos_ratio0_log != string::npos){
				pos_ratio0_log+=39;
				sscanf(conffile.substr(pos_ratio0_log).c_str(), "%d", &ratio0_lo[1]);
				if(pf)printf("gamma ratio0_lo (100*ln(rat)) =%d\n",ratio0_lo[1]);
			}else {printf("Error in gamma ratio0_lo. Delete file to regenerate from template.\n"); exit(0);}
		size_t pos_ratio0_upg= conffile.find("Upper t1/amp gamma ratio (100*ln(rat)):");
			if (pos_ratio0_upg != string::npos){
				pos_ratio0_upg+=39;
				sscanf(conffile.substr(pos_ratio0_upg).c_str(), "%d", &ratio0_up[1]);
				if(ratio0_up[1]<=ratio0_lo[1]) {printf("Error: gamma ratio0_up<=ratio0_lo.\n"); exit(0);}
				if(pf)printf("gamma ratio0_up (100*ln(rat)) =%d\n",ratio0_up[1]);
			}else {printf("Error in gamma ratio0_up. Delete file to regenerate from template.\n"); exit(0);}
		size_t pos_ratio1_log= conffile.find("Lower t1/t2 gamma ratio (100*ln(rat)):");
			if (pos_ratio1_log != string::npos){
				pos_ratio1_log+=38;
				sscanf(conffile.substr(pos_ratio1_log).c_str(), "%d", &ratio1_lo[1]);
				if(pf)printf("gamma ratio1_lo (100*ln(rat)) =%d\n",ratio1_lo[1]);
			}else {printf("Error in gamma ratio1_lo. Delete file to regenerate from template.\n"); exit(0);}
		size_t pos_ratio1_upg= conffile.find("Upper t1/t2 gamma ratio (100*ln(rat)):");
			if (pos_ratio1_upg != string::npos){
				pos_ratio1_upg+=38;
				sscanf(conffile.substr(pos_ratio1_upg).c_str(), "%d", &ratio1_up[1]);
				if(ratio1_up[1]<=ratio1_lo[1]) {printf("Error: gamma ratio1_up<=ratio1_lo.\n"); exit(0);}
				if(pf)printf("gamma ratio1_up (100*ln(rat)) =%d\n",ratio1_up[1]);
			}else {printf("Error in gamma ratio1_up. Delete file to regenerate from template.\n"); exit(0);}
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
			
		if(pf)printf("All loaded, no errors (I did not check for boundaries, you better had chosen them properly)!.\n");
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
	if(pf)printf ("Note that existing .dat files are read and new counts are added to existing ones. If the settings change (such as energy boundaries) these files should be removed"
	              ", else the program may crash (because wrong file lenghts etc.).\n\n");
	_load_conf(pf);
	alpha_mintime_uint=(unsigned)(alpha_mintime*125000000);
	gamma_mintime_uint=(unsigned)(gamma_mintime*125000000);
	interval_uint=(unsigned)(interval*125000000);

	system ("mkdir measurements -p");
	system ("cat red_pitaya.bin > /dev/xdevcfg");
	if(pf){	printf("Press any key to continue...\n");
		scanf("%*c");
	}
	
	if (AGC_init()) return -1;		//fpga init
	AGC_setup(alpha_thresh,gamma_thresh,alpha_edge,gamma_edge,alpha_mintime_uint,gamma_mintime_uint,delay_len,delay_ch);
	
	time_t Tstart,Tend;
	if(pf){
		thread term_thread (term_fun);			//ending by button
		term_thread.detach();
		time(&Tstart);
	}else{
		thread end_thread (end_fun,atof(argv[1]));	//ending by timer
        	end_thread.detach();
	}
       
        FILE* ifile;
		//####generate alpha energy array
	int ENmax_alpha;
	if (!alpha_edge){	//rising edge
		ENmax_alpha=8191-alpha_thresh+1;	//num of elements in the array
	}else{
		ENmax_alpha=-(-8192-alpha_thresh)+1;	//num of elements in the array
	}
	ifile = fopen("measurements/alpha.dat","rb");
	unsigned *alpha_array = new unsigned[ENmax_alpha];
	if (ifile!=NULL) {
		fread (alpha_array,sizeof(unsigned),ENmax_alpha,ifile);	// read existing file
		fclose(ifile);
	}
	else for (int i=0;i!=ENmax_alpha;i++)alpha_array[i]=0;	// else fill with 0
	unsigned alpha_binN = ENmax_alpha/step_alpha+1;
	
		//####generate gamma energy array
	int ENmax_gamma;
	if (!gamma_edge){
		ENmax_gamma=8191-gamma_thresh+1;	//num of elements in the array
	}else{
		ENmax_gamma=-(-8192-gamma_thresh)+1;	//num of elements in the array
	}
	ifile = fopen("measurements/gamma.dat","rb");
	unsigned *gamma_array = new unsigned[ENmax_gamma];
	if (ifile!=NULL) {
		fread (gamma_array,sizeof(unsigned),ENmax_gamma,ifile);	// read existing file
		fclose(ifile);
	}
	else for (int i=0;i!=ENmax_gamma;i++)gamma_array[i]=0;	// else fill with 0
	unsigned gamma_binN = ENmax_gamma/step_gamma+1;
	
		//####generate alpha ratio0 array
	ifile = fopen("measurements/alpha_ratio0.dat","rb");
	unsigned *alpha_ratio0_array = new unsigned[2000];
	if (ifile!=NULL) {
		fread (alpha_ratio0_array,sizeof(unsigned),2000,ifile);	// read existing file
		fclose(ifile);
	}
	else for (int i=0;i!=2000;i++)alpha_ratio0_array[i]=0;	// else fill with 0
	
		//####generate alpha rratio1 array
	ifile = fopen("measurements/alpha_ratio1.dat","rb");
	unsigned *alpha_ratio1_array = new unsigned[2000];
	if (ifile!=NULL) {
		fread (alpha_ratio1_array,sizeof(unsigned),2000,ifile);	// read existing file
		fclose(ifile);
	}
	else for (int i=0;i!=2000;i++)alpha_ratio1_array[i]=0;	// else fill with 0
		
		//####generate gamma ratio0 array
	ifile = fopen("measurements/gamma_ratio0.dat","rb");
	unsigned *gamma_ratio0_array = new unsigned[2000];
	if (ifile!=NULL) {
		fread (gamma_ratio0_array,sizeof(unsigned),2000,ifile);	// read existing file
		fclose(ifile);
	}
	else for (int i=0;i!=2000;i++)gamma_ratio0_array[i]=0;	// else fill with 0
	
		//####generate gamma rratio1 array
	ifile = fopen("measurements/gamma_ratio1.dat","rb");
	unsigned *gamma_ratio1_array = new unsigned[2000];
	if (ifile!=NULL) {
		fread (gamma_ratio1_array,sizeof(unsigned),2000,ifile);	// read existing file
		fclose(ifile);
	}
	else for (int i=0;i!=2000;i++)gamma_ratio1_array[i]=0;	// else fill with 0
	
		//####generate time arrays
	unsigned*** bins = new unsigned**[alpha_binN];
	for (int i=0; i!=alpha_binN;i++) bins[i] = new unsigned*[gamma_binN];
	
	for (int i=0; i!=alpha_binN;i++) for (int j=0; j!=gamma_binN;j++){
		bins[i][j] = new unsigned[interval_uint];
		string fname = "measurements/a"+to_string(i)+"g"+to_string(j)+".dat";
		ifile = fopen(fname.c_str(),"rb");
		if (ifile!=NULL) {
			fread (bins[i][j],sizeof(unsigned),interval_uint,ifile);	// read existing file
			fclose(ifile);
		}
		else for (int k=0;k!=interval_uint;k++)bins[i][j][k]=0;	// else fill with 0
	}
	
	long long unsigned N_alpha=0;
        long long unsigned N_gamma=0;		//on PC long long int is 8byte, long int is 8byte, int is 4byte
        long long unsigned counter=0;		//on ARM long long int is 8byte, long int is 4byte, int is 4byte	//TODO maybe use uint32_t 
        long long unsigned event_ts;
        
        deque <peak> active_trig;
        bool isalpha;
	int amplitude;
	unsigned int cntr_t0;
	unsigned int cntr_t1;
	unsigned int cntr_t2;
	int ratio[2];
	bool isgood;
	
	AGC_reset_fifo(); 
	for(int i=0;;i++){
		if (!AGC_get_sample(&isalpha,&amplitude,&cntr_t0,&cntr_t1,&cntr_t2)){
			if (isalpha){
				N_alpha++;
				ratio[0]=100*log(cntr_t1/(abs(amplitude-alpha_thresh)+1));
				if (ratio[0]< -1000) ratio[0]=-1000;
				else if (ratio[0]>1000) ratio[0]=1000;
				ratio[1]=100*log(cntr_t1/(cntr_t0+1));
				if (ratio[0]< -1000) ratio[0]=-1000;
				else if (ratio[0]>1000) ratio[0]=1000;
				
				alpha_ratio0_array[1000+ratio[0]]++;
				alpha_ratio1_array[1000+ratio[1]]++;
				
				if (ratio[0]>=ratio0_lo[0] && ratio[0]<=ratio0_up[0] && ratio[1]>=ratio1_lo[0] && ratio[1]<=ratio1_up[0]){
					alpha_array[abs(amplitude-alpha_thresh)]++;
					isgood=true;
				}
				else isgood=false;
					
			}
			else{
				N_gamma++;
				ratio[0]=100*log(cntr_t1/(abs(amplitude-alpha_thresh)+1));
				if (ratio[0]< -1000) ratio[0]=-1000;
				else if (ratio[0]>1000) ratio[0]=1000;
				ratio[1]=100*log(cntr_t1/(cntr_t0+1));
				if (ratio[0]< -1000) ratio[0]=-1000;
				else if (ratio[0]>1000) ratio[0]=1000;
				
				gamma_ratio0_array[1000+ratio[0]]++;
				gamma_ratio1_array[1000+ratio[1]]++;
				
				if (ratio[0]>=ratio0_lo[1] && ratio[0]<=ratio0_up[1] && ratio[1]>=ratio1_lo[1] && ratio[1]<=ratio1_up[1]){
					gamma_array[abs(amplitude-gamma_thresh)]++;
					isgood=true;
				}
				else isgood=false;
			}
			
			if (isgood){
				event_ts=counter+cntr_t0-cntr_t1;
				if (triggerisalpha == isalpha){
					active_trig.emplace_back();
					active_trig.back().time=event_ts;
					active_trig.back().amp=amplitude;
					active_trig.back().ratio[0]=ratio[0];
					active_trig.back().ratio[1]=ratio[1];
				}else{	//the other is trigger
					for (int j=0;j!=active_trig.size();j++){
						if (event_ts>active_trig[j].time+interval_uint){
							active_trig.pop_front();
							j--;
						}else{
							unsigned a,b;
							if (active_trig[j].time<=event_ts) {
								a=abs(amplitude-alpha_thresh)/step_alpha;
								b=abs(amplitude-gamma_thresh)/step_gamma;
								bins[a][b][event_ts-active_trig[j].time]++;
							}
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
	
	double vtime;
	if (pf) {
		time(&Tend);
		vtime = difftime(Tstart,Tend);
	}
	else vtime=atof(argv[1]);
	
	if(pf)printf ("\033[2JAcquistion ended.\nN_alpha=%llu\nN_gamma=%llu\nelapsed time=%llu s\nRPTY lost peaks:%u(max in queue %u/200)\n",N_alpha,N_gamma,counter/125000000,AGC_get_num_lost(),AGC_get_max_in_queue());
	
	FILE* ofile;
	
	if(pf)printf("Saving alpha...");
	ofile=fopen("measurements/alpha.dat","wb");
	fwrite (alpha_array,sizeof(unsigned),ENmax_alpha,ofile);
	fclose(ofile);
	if(pf)printf("done! format is '%uint32' starting from zero. One line is one channel.\n");
	delete[] alpha_array;

	if(pf)printf("Saving gamma...");
	ofile=fopen("measurements/gamma.dat","wb");
	fwrite (gamma_array,sizeof(unsigned),ENmax_gamma,ofile);
	fclose(ofile);
	if(pf)printf("done! format is '%uint32' starting from zero. One line is one channel.\n");
	delete[] gamma_array;

	if(pf)printf("Saving alpha_ratio0...");
	ofile=fopen("measurements/alpha_ratio0.dat","wb");
	fwrite (alpha_ratio0_array,sizeof(unsigned),2000,ofile);
	fclose(ofile);
	if(pf)printf("done! format is '%uint32' starting from -1000. One line is one ratio, up to 1000.\n");
	delete[] alpha_ratio0_array;
	
	if(pf)printf("Saving alpha_ratio1...");
	ofile=fopen("measurements/alpha_ratio1.dat","wb");
	fwrite (alpha_ratio1_array,sizeof(unsigned),2000,ofile);
	fclose(ofile);
	if(pf)printf("done! format is '%uint32' starting from -1000. One line is one ratio, up to 1000\n");
	delete[] alpha_ratio1_array;
	
	if(pf)printf("Saving gamma_ratio0...");
	ofile=fopen("measurements/gamma_ratio0.dat","wb");
	fwrite (gamma_ratio0_array,sizeof(unsigned),2000,ofile);
	fclose(ofile);
	if(pf)printf("done! format is '%uint32' starting from -1000. One line is one ratio, up to 1000\n");
	delete[] gamma_ratio0_array;
	
	if(pf)printf("Saving gamma_ratio1...");
	ofile=fopen("measurements/gamma_ratio1.dat","wb");
	fwrite (gamma_ratio1_array,sizeof(unsigned),2000,ofile);
	fclose(ofile);
	if(pf)printf("done! format is '%uint32' starting from -1000. One line is one ratio, up to 1000\n");
	delete[] gamma_ratio1_array;
	
	if(pf)printf("Saving time...");
	for (int i=0; i!=alpha_binN;i++) {
		for (int j=0; j!=gamma_binN;j++){
			string fname = "measurements/a"+to_string(i)+"g"+to_string(j)+".dat";
			ofile = fopen(fname.c_str(),"wb");
			fwrite (bins[i][j],sizeof(unsigned),interval_uint,ofile);
			fclose(ofile);
			delete[] bins[i][j];
		}
		delete[] bins[i];
	}
	delete[] bins;
	if(pf)printf("done! format is '%uint32' starting from 0. One line is 8ns.\n");
	
	if(pf)printf("Saving duration...");
	ofile=fopen("measurements/duration.txt","a");
	fprintf(ofile,"+%lf seconds\n",vtime);
	fclose(ofile);
	
	AGC_exit();
}
