#include <iostream>
#include <unistd.h>
#include <vector>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <algorithm>
#include <cstring>
#include <numeric>

using namespace std;

// -------------------- Data Structures -------------------------
struct ProcessControlBlock {
    int id;
    pid_t pid;
    int readPipe[2];   // Child -> parent
    int writePipe[2];  // Parent -> child
    int deadline;
    int remainingTime;
    int finished = 0;
    vector<int> allocation, maximum, need;
};

struct Request {
    string cmd;
    vector<int> numbers;
};

// is system in safe state after tentative grant?
bool isSafe(vector<int> available,
            vector<ProcessControlBlock>& procs,
            vector<int> req,
            int idx) {
    int m = available.size(), n = procs.size();
    // simulate grant on copy
    vector<int> work = available;
    for (int j = 0; j < m; j++) {
        work[j] -= req[j];
        procs[idx].allocation[j] += req[j];
        procs[idx].need[j] -= req[j];
    }
    vector<bool> finish(n,false);
    while(true){
        bool progress = false;
        for(int i=0;i<n;i++){
            if (!finish[i]) {
                bool can=true;
                for(int j=0;j<m;j++) if (procs[i].need[j]>work[j]) can=false;
                if (can) {
                    for(int j=0;j<m;j++) work[j] += procs[i].allocation[j];
                    finish[i]=progress=true;
                }
            }
        }
        if (!progress) break;
    }
    // undo simulation
    for (int j=0;j<m;j++) {
        procs[idx].allocation[j] -= req[j];
        procs[idx].need[j] += req[j];
    }
    return all_of(finish.begin(),finish.end(),[](bool x){return x;});
}

Request parse_req(string line){
    istringstream iss(line);
    string cmd; iss >> cmd;
    Request req{cmd};
    int num;
    while(iss >> num) req.numbers.push_back(num);
    return req;
}

// -------------------- Main Function  ------------------------
int main() {
    int m, n;
    cin >> m >> n;
    vector<int> available(m);
    for (int &x: available) cin >> x;
    vector<vector<int>> maximum(n,vector<int>(m));
    for (auto &row: maximum) {
        for (int &x: row) cin >> x;
    }
    vector<int> deadlines(n), compTimes(n);
    vector<vector<Request>> instructions(n);

    for(int p=0;p<n;p++){
        string dummy;
        cin >> dummy; // process_x:
        cin >> deadlines[p] >> compTimes[p];
        cin.ignore();
        string line;
        while(getline(cin,line)){
            line.erase(line.find_last_not_of(" \r\n\t;")+1);
            if(line=="end.") break;
            instructions[p].push_back(parse_req(line));
        }
    }

    // BANKER+EDF with fork/pipes
    vector<ProcessControlBlock> procs(n);

    for(int i=0;i<n;i++){
        pipe(procs[i].readPipe);
        pipe(procs[i].writePipe);
        procs[i].id=i;
        procs[i].maximum=maximum[i];
        procs[i].allocation=vector<int>(m,0);
        procs[i].need=maximum[i];
        procs[i].deadline=deadlines[i];
        procs[i].remainingTime=compTimes[i];

        pid_t pid=fork();
        if(pid==0){ // CHILD
            close(procs[i].writePipe[1]); // parent write end
            close(procs[i].readPipe[0]);  // parent read end

            int step=0;
            while(step < (int)instructions[i].size()){
                stringstream ss;
                ss << instructions[i][step].cmd;
                for(int num: instructions[i][step].numbers) ss<<" "<<num;
                string instr = ss.str() + "\n";
                write(procs[i].readPipe[1],instr.c_str(),instr.size());
                char buf[1024]={0};
                read(procs[i].writePipe[0],buf,sizeof(buf));
                if(strstr(buf,"done")!=nullptr){
                    step++;
                }
                // else, wait blocked...
            }
            close(procs[i].writePipe[0]);
            close(procs[i].readPipe[1]);
            exit(0);
        } else {
            procs[i].pid=pid;
            close(procs[i].writePipe[0]);
            close(procs[i].readPipe[1]);
        }
    }

    int alive=n, time=0;
    vector<bool> deadlineMissed(n,false);
    while(alive){
        int sel=-1;
        int minD=1e9;
        int minRem=1e9;
        for(int i=0;i<n;i++){
            if(procs[i].finished) continue;
            if(procs[i].deadline < time && !deadlineMissed[i]){
                cout<<"Missed deadline for process "<<i+1<<" at time "<<time<<"\n";
                deadlineMissed[i]=true;
            }
            if(procs[i].deadline<minD || (procs[i].deadline==minD && procs[i].remainingTime<minRem)){
                minD=procs[i].deadline;
                minRem=procs[i].remainingTime;
                sel=i;
            }
        }
        if(sel==-1) break; // done
        char buf[1024]={0};
        int len=read(procs[sel].readPipe[0],buf,sizeof(buf));
        if(len<=0){ procs[sel].finished=1; alive--; continue;}
        string instr(buf);
        Request req=parse_req(instr);
        bool advance=true;
        if(req.cmd=="compute"){
            int t=req.numbers[0];
            procs[sel].remainingTime-=t;
            time+=t;
            write(procs[sel].writePipe[1],(char*)"done",4);
        }else if(req.cmd=="request"){
            vector<int> request=req.numbers;
            bool valid=true;
            for(int j=0;j<m;j++)
                if(request[j]>procs[sel].need[j] || request[j]<0) valid=false;
            bool enough = valid && [&]{
                for(int j=0;j<m;j++) if(request[j]>available[j]) return false;
                return true;
            }();
            if(valid && enough && isSafe(available,procs,request,sel)){
                for(int j=0;j<m;j++){
                    available[j]-=request[j];
                    procs[sel].allocation[j]+=request[j];
                    procs[sel].need[j]-=request[j];
                }
                procs[sel].remainingTime--;
                cout<<"Granted request for P"<<sel+1<<" at time "<<time<<"\n";
            } else {
                cout<<"Denied/blocked request for P"<<sel+1 <<" at time "<<time<<"\n";
                advance=false;
            }
            write(procs[sel].writePipe[1],(char*)(advance?"done":"blocked"),advance?4:7);
            if(advance) time++;
        }else if(req.cmd=="release"){
            vector<int> r=req.numbers;
            for(int j=0;j<m;j++){
                available[j]+=r[j];
                procs[sel].allocation[j]-=r[j];
                procs[sel].need[j]+=r[j];
            }
            procs[sel].remainingTime--;
            time++;
            write(procs[sel].writePipe[1],(char*)"done",4);
        }else if(req.cmd=="use_resources"||req.cmd=="reduce_resources"){
            int x=req.numbers[0], y=req.numbers[1];
            procs[sel].remainingTime-=x;
            time+=x;
            write(procs[sel].writePipe[1],(char*)"done",4);
        }else if(req.cmd=="print_resources_used"){
            time++;
            procs[sel].remainingTime--;
            write(procs[sel].writePipe[1],(char*)"done",4);
        }else { // end
            write(procs[sel].writePipe[1],(char*)"done",4);
        }
        // report state
        cout<<"Available: ";
        for(int x: available) cout<<x<<" ";
        cout<<"\n";
    }

    for(int i=0;i<n;i++) wait(NULL);
    cout<<"Simulation done at time "<<time<<"\n";
}
