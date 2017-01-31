#include <iostream>
#include <fstream>
#include <string>
#include <queue>
#include <vector>
#include <deque>
#include <map>
#include <iomanip>
#include <algorithm>
#include <numeric>
using namespace std;

struct Gate
{
  string ID;  //u23
  string type;  //AND
  double inputcap;  //from NLDM
  double loadcap;  //sum of fanout caps
  vector<double> delay; //from LUT
  vector<double> input_slew;  //first entry
  double output_slew;  //from LUT
  vector<Gate> input;
  vector<Gate> fanout;
  vector<double> inp_arrival;
  double out_arrival;
};
queue <Gate> Q, Qmask;
double invertercap;
map<string, double**> All_delay, All_slew;
vector <string> Allgate_name;
double Cload_vals[7];
double Tau_in_vals[7];
vector <string> vin, vout;  //vectors for storing the inputs/outputs
istream& parse_delay(istream& fin, string& s);
ostream& display_delay(ostream& fout, map<string, double**> All_delay, double Cload_vals[], double Tau_in_vals[], string s);
istream& parse_slew(istream& fin, string& s);
ostream& display_slew(ostream& fout, map<string, double**> All_slew, double Cload_vals[], double Tau_in_vals[], string s);

vector<Gate> SearchinQueue(Gate g, queue<Gate> v);
void setInputCap(double cap, queue<Gate> q, string type);
void setInSlew(double tau[7], queue<Gate> q, string type);
void setAllLoadCap( );
void setinputSlew(vector<string> vin);
double findloadcap(Gate g);
double findActualDelay(double cap, double slew, string s);
double findInputSlew(double cap, double slew, string s);
void node(string name, string a);
Gate SearchForType(Gate target, queue<Gate> q);
bool isInput(vector<string> vin, Gate g);
ostream& displayFanOUT(queue<Gate> Q, ostream& out);
ostream& displayFanIN(queue<Gate> Q, ostream& out, vector<string> vin, queue<Gate> Qm);

void node(string filename, string outputfile)
{
  ifstream in;
  ofstream out;
  char c;
  int len = 0;
  int num_not = 0, num_nand = 0, num_or = 0, num_and = 0,
      num_nor = 0, num_xor = 0, num_buf = 0;
  int pos, num_in, num_out;
  string s, s1, s2, first_input, sc;
  struct Gate g,child_g, output_g;
  vector<Gate>::iterator it;
  vector<Gate> vg;
  in.open (filename.c_str());
  if(in.fail())
  {
    cout << "no existing ckt file to read...\n ";
    exit(1);
  }
  out.open(outputfile.c_str());
  do
  {
    in >> std::ws;
    getline(in,s); //get rid of comments
  } while(s[0] == '#');
  //at this point, s contain the first input line
  pos = s.find("(") + 1;
  first_input = s.substr(pos,s.find(")")-pos);
  vin.push_back(first_input);

  while(s[0] == 'I')  //parse the inputs
  {
      in >> std::ws;
      if((c = in.peek()) != 'I') break;
    getline(in,s,'('); //s="input"
    getline(in,s1,')');  //s1="n1"
    vin.push_back(s1);
  }
  num_in = vin.size(); out << num_in << " primary inputs" << endl;

  do   //parse the outputs
  {
      in >> std::ws;
      if((c = in.peek()) != 'O') break;
    getline(in,s,'('); //s="output"
    getline(in,s1,')'); //s1="U35"
    vout.push_back(s1);
  } while(s[0] == 'O');
  num_out = vout.size();   out << num_out << " primary outputs" << endl;

  //parse the gate information
  do
  {
    in >> std::ws;
    g.input.clear();
    if(in.eof()) break;
    getline(in,s,'=');   //s = U34
    for (int i = s.length()-1; i >= 0; --i) {
       if(s[i] == ' ')
          s.erase(i, 1);
    }
    in >> std::ws;
    g.ID = s;
    getline(in,s1,'(');   //s1 = NAND
    g.type = s1;
    in >> std::ws;
    if(s1 == "NOT" || s1 == "BUFF" || s1 == "BUF")  //is a one-input gate
    {
      getline(in,s2,')');   //s2 = gate inputs
      child_g.ID = s2;
      g.input.push_back(child_g); //add the input node to vector
      Q.push(g);
    }
    else  //is a n-input gate
    {
      getline(in,s,')');  //out << s << endl;
      sc = s;len =0;
      do
      {
        in >> std::ws;
        s2 = s.substr(0,s.find(","));  //s2 = U38
        s.erase(0,s2.length()+2);
        len += s2.length()+2;
        child_g.ID = s2;
        g.input.push_back(child_g);
      }  while(len < (signed)sc.length());

      Q.push(g);
    }

    if(s1 == "NAND") num_nand ++;
    if(s1 == "AND") num_and ++;
    if(s1 == "NOR") num_nor ++;
    if(s1 == "NOT") num_not ++;
    if(s1 == "OR") num_or ++;
    if(s1 == "XOR") num_xor ++;
    if(s1 == "BUF" || s1 == "BUFF") num_buf ++;
  } while(!in.eof());

  out << num_nand << " NAND gates\n" << num_or << " OR gates\n" << num_and
      << " AND gates\n" << num_xor << " XOR gates\n" << num_not << " NOT gates\n"
      << num_buf << " BUF gates\n";

  Qmask = Q;
  /*\the following displays the queue information
   while(!Qmask.empty())
   {
     cout << Qmask.front().slew << endl;
     Qmask.pop();
   }
   */

  //display Fanout...

  out << "Fanout...\n";
  displayFanOUT( Q,  out);
  //cout << Q.front().fanout.size() << endl;
  //Q = Qmask;
  //display Fanin...
  out << "Fanin...\n";
  displayFanIN( Q, out, vin, Qmask);
  in.close();
  out.close();
}

Gate SearchForType(Gate target, queue<Gate> q)
{
  queue <Gate> qm;
  Gate g, temp;
  qm = q;
  while(!qm.empty())
  {
    g = qm.front();
    if(target.ID == g.ID)
      return g;
    qm.pop();
  } return temp;
}

vector<Gate> SearchinQueue(Gate target, queue<Gate> q)
{
 // vector<Gate>::iterator it;
  Gate g;
  std::vector<Gate> dummy;
  while(!q.empty())
  {
    g = q.front();
    for(int i = 0; i < (signed)g.input.size();i++)
    {
      if(target.ID == g.input[i].ID)  //found target in the input vector
      {
        dummy.push_back(g);
      }
    }
    q.pop();
  }
  return dummy;
}

bool isInput(vector<string> vin, Gate g)
{
 bool flag = false;
 for(int i = 0; i < (signed)vin.size(); i ++)
    if(vin[i] == g.ID)
      flag = true;
  return flag;
}

double findloadcap(Gate g)
{
  queue<Gate> copy = Q;
  //cout << "aaaaa " << g.inputcap << endl;
  while(!copy.empty())
  {
    if(copy.front().ID == g.ID)
      return copy.front().inputcap;
    copy.pop();
  }
  return 0;
}

void setAllLoadCap( )
{
  double sum = 0;
  Gate g;
  queue<Gate> qm = Q, temp_q; //cout << qm.size() << endl;
  while(!qm.empty())
  {
    g = qm.front();
    if(g.fanout.size() == 0)  //it's an OUTP
      g.loadcap = 4*invertercap;
    else
    {
      for(int i = 0; i < g.fanout.size(); i++)
      {
        g.fanout[i].inputcap = findloadcap(g.fanout[i]);
        sum += g.fanout[i].inputcap;
      }
      g.loadcap = sum;
      sum = 0;
    }
    qm.pop();
    temp_q.push(g);
  }

  Q = temp_q;
}

ostream& displayFanOUT(queue<Gate> QQ, ostream& out)
{
  Gate g;
  queue<Gate> copy;
  vector<Gate> vg;
  while(!QQ.empty())
  {
    g = QQ.front();
    out << g.type << "-" << g.ID << ": " ;
    vg = SearchinQueue(g,QQ);
    g.fanout = vg;  //cout << g.fanout.size()<<endl;
    if(vg.size())
    {
      for(int i = 0; i < (signed)vg.size()-1; i++)
          out << g.fanout[i].type << "-" << g.fanout[i].ID << ", ";
      out << g.fanout[vg.size()-1].type << "-" << g.fanout[vg.size()-1].ID << endl;
    }
    else
    {
      out << "OUTP" << endl;
    }
    QQ.pop();
    copy.push(g);
  }
  Q = copy; // cout << Q..fanout.size()<<endl;
  return out;
}

ostream& displayFanIN(queue<Gate> Q, ostream& out, vector<string> vin, queue<Gate> Qmask)
{
  Gate g;
  while(!Q.empty())
  {
    g = Q.front();
    string s = "INP-";//out << g.ID << '\t' << g.input[0].ID << endl;
    if(!isInput(vin, g.input[0]))
    {
      s = SearchForType(g.input[0],Qmask).type + "-";
    }
    out << g.type << "-" << g.ID << ": " << s << g.input[0].ID;
    if(g.type != "NOT")
    {
      for(int i = 1; i < (signed)Q.front().input.size(); i++)
      {
        s = "INP-";
        if(!isInput(vin, g.input[i]))
           s = SearchForType(g.input[i],Qmask).type + "-";
        out << ", " << s << g.input[i].ID << ' ';
      }
    } out << endl;
    Q.pop();
  }
  return out;
}

//set a type of gate's capacitance
void setInputCap(double cap, queue<Gate> q, string type)
{
  Gate g;
  queue<Gate> copy;
  if(type == "NAND2_X1")
      type = "NAND";
  if(type == "NOR2_X1")
      type = "NOR";
  if(type == "AND2_X1")
      type = "AND";
  if(type == "OR2_X1")
      type = "OR";
  if(type == "XOR_X1")
      type = "XOR";
  if(type == "INV_X1")
      type = "NOT";

  while(!q.empty())
  {
    g = q.front();
    if(g.type == type)
    {
      g.inputcap = cap;
    }
    if(type == "NOT")
      invertercap = cap;
    q.pop();
    copy.push(g);
  }
  Q = copy;
}

void setinputSlew(vector<string> vin)
{
  Gate g;
  queue<Gate> q = Q, copy;
  while(!q.empty())
  {
    g = q.front(); cout << g.ID << g.type << endl;
    for(int i = 0; i < g.input.size(); i++)
    {
      if(isInput(vin, g.input[i]))
      {
        g.input_slew.push_back(0.002);
        cout << "primary gates\n";  //TODO: seg fault when not a primary input
        //INP set input slew to be 2ps
      }
      else
      {
        g.input[i] = SearchForType(g.input[i], Q); //TODO:T_OUT = ARGMAX(AI+DI)
         cout << g.input[i].output_slew << "else...\n";
        g.input_slew.push_back(findInputSlew(g.loadcap, g.input[i].output_slew,g.type));
      }
    }
    q.pop();
    copy.push(g);
  }
  Q = copy;
}

double findActualDelay(double cap, double slew, string s, map<string, double**> All_delay)
{
  double c1,c2,t1,t2,delay;
  int i,j;
  for(i = 0; i < 7; i++)
  {
    if(Tau_in_vals[i]<= slew && Tau_in_vals[i+1] > slew)
    {
      c1 = Tau_in_vals[i];
      c2 = Tau_in_vals[i+1];
      break;
    }
  }
  for(j = 0; j < 7; j++)
  {
    if(Cload_vals[j]<= cap && Cload_vals[j+1] > cap)
    {
      t1 = Cload_vals[j];
      t2 = Cload_vals[j+1];
      break;
    }
  }
  if(s != "NOT")
    s += "2_X1";
  else
    s = "INV_X1";

  double** temp = All_delay[s];
  //cout << "in findActualDelay..." << s << endl;
  //cout << All_delay[s]; //TODO: problem is seg fault here; temp has valid addr, temp[]
  delay = (temp[i][j]*(c2-cap)*(t2-slew)
    + temp[i][j+1]*(cap-c1)*(t2-slew)
    + temp[j+1][i]*(c2-cap)*(slew-t1)
    + temp[i+1][i+1]*(cap-c1)*(slew-t1))/(c2-c1)/(t2-t1); cout << "delay is "<< delay << endl;
  if(delay < 0)
    delay = 0;
  return delay;
}

void setDelay(map<string, double**> All_delay)
{
  queue<Gate> q = Q, copy;
  Gate g;
  while(!q.empty())
  {
    g = q.front();
    for(int i = 0; i < g.input.size(); i++)
    { //cout << g.ID << g.type << endl;
      g.delay.push_back(findActualDelay(g.loadcap, g.input_slew[i], g.type, All_delay));
   //   g.delay.push_back(0.0014);
    }
    q.pop();
    copy.push(g);
  }
  Q = copy;
}

double findInputSlew(double cap, double slew, string s)
{
  double c1,c2,t1,t2,tau;
  int i,j;
  for(i = 0; i < 7; i++)
  {
    if(Tau_in_vals[i]< slew && Tau_in_vals[i+1] > slew)
    {
      c1 = Tau_in_vals[i];
      c2 = Tau_in_vals[i+1];
      break;
    }
  }
  for(j = 0; j < 7; j++)
  {
    if(Cload_vals[j]< cap && Cload_vals[j+1] > cap)
    {
      t1 = Cload_vals[j];
      t2 = Cload_vals[j+1];
      break;
    }
  }

  slew = (All_slew.find(s)->second[i][j]*(c2-cap)*(t2-slew)
    + All_slew.find(s)->second[i][j+1]*(cap-c1)*(t2-slew)
    + All_slew.find(s)->second[j+1][i]*(c2-cap)*(slew-t1)
    + All_slew.find(s)->second[i+1][i+1]*(cap-c1)*(slew-t1))/(c2-c1)/(t2-t1);
  if(tau < 0)
    tau = 2;
  return tau;
}


istream& parse_delay(istream& fin, string& name)
{
 string s,line,capline,dummy;
 char dummy_c;
 double cap;
 double solution[7][7];
 double* solutionPtrs[7];
 double** t = new double*[7];
 ofstream fout;
 fout.open("delay_LUT.txt",std::ofstream::out | std::ofstream::app);
   //some code to remove the comments
 while ( getline (fin,line) )
 {
    std::remove_if(line.begin(), line.end(), ::isspace);
    if((s = line.substr(0,4))== "cell")
      break;
 }
 if(fin.eof()) return fin;
 s = line.substr(5,line.find(")") - line.find("(") - 1);  // s = "NOR_X1"

 name = s;
 Allgate_name.push_back(s);
 fin >> capline;
 std::remove_if(capline.begin(), capline.end(), ::isspace); //fout << capline << endl; //capline = "capacitance   : 1.599032;"
 fin >> dummy_c >> cap >> dummy_c;
 setInputCap(cap, Q, s);
 getline(fin, dummy, '\"');
 fin >> dummy_c;
 for(int i = 0; i < 7; i++)
 {
    fin >> Tau_in_vals[i] >> dummy_c;

 }
 //setInSlew(Tau_in_vals, Q, s);
 getline(fin, dummy, '\"');
 fin >> dummy_c;
 for(int i = 0; i < 7; i++)
    fin >> Cload_vals[i] >> dummy_c;
 getline(fin, dummy, '\"');
 fin >> dummy_c;
 for(int i = 0; i < 7; i++)
 {
   for(int j = 0; j < 7; j++)
   {
      fin >> solution[i][j] >> dummy_c;
   }
   getline(fin, dummy);
   fin>>dummy_c;
 }
 for (int i = 0; i < 7; ++i)
 {
    solutionPtrs[i] = solution[i];
    t[i] = new double[7];
 }
 for(int i = 0; i < 7; i++)
 {
   for(int j = 0; j < 7; j++)
   {
     t[i][j] = solution[i][j];
   }
 }
 //t = solutionPtrs; //cout << t << endl;
 //cout << s << endl;
 All_delay.insert( std::pair<string, double**>(s, t) );
 display_delay(fout, All_delay, Cload_vals, Tau_in_vals, s);
 fout.close();
 return fin;
}

ostream& display_delay(ostream& fout, map<string, double**> All_delay, double Cload_vals[], double Tau_in_vals[], string s)
{
 fout << "cell: " << s << endl;
 fout << "input slews:\n";
 for(int i = 0; i < 6; i++)
 {
    fout << Tau_in_vals[i] << ',';
 }
 fout << Tau_in_vals[6] << endl;
 fout << "load caps:\n";
 for(int i = 0; i < 6; i++)
 {
    fout << Cload_vals[i] << ',';
 }
 fout << Cload_vals[6] << endl << endl;
 double** temp = All_delay[s];
fout << "delays:\n";//fout << (*iter).first << " is:\n";
for(int i = 0; i < 7; i++)
{
   for(int j = 0; j < 7; j++)
   {
      fout << std::fixed << std:: setprecision(8) << std::setw(8) << temp[i][j] << '\t';
   }  fout << endl;
}

 fout << endl << endl << endl << endl;
 return fout;
}

istream& parse_slew(istream& fin, string& name)
{
  string s,line,capline,dummy;
 char dummy_c;
 double cap;
 double solution[7][7];
 double* solutionPtrs[7];
 double** t = new double*[7];
 ofstream fout;
 fout.open("slew_LUT.txt",std::ofstream::out | std::ofstream::app);
   //some code to remove the comments

 getline(fin, dummy, '\"');
 fin >> dummy_c;
 for(int i = 0; i < 7; i++)
 {
    fin >> Tau_in_vals[i] >> dummy_c;

 }
 //setInSlew(Tau_in_vals, Q, s);
 getline(fin, dummy, '\"');
 fin >> dummy_c;
 for(int i = 0; i < 7; i++)
    fin >> Cload_vals[i] >> dummy_c;
 getline(fin, dummy, '\"');
 fin >> dummy_c;
 for(int i = 0; i < 7; i++)
 {
   for(int j = 0; j < 7; j++)
   {
      fin >> solution[i][j] >> dummy_c;
   }
   getline(fin, dummy);
   fin>>dummy_c;
 }
 for (int i = 0; i < 7; ++i)
 {
    solutionPtrs[i] = solution[i];
    t[i] = new double[7];
 }
 for(int i = 0; i < 7; i++)
 {
   for(int j = 0; j < 7; j++)
   {
     t[i][j] = solution[i][j];
   }
 }
 fout << "cell: " << name << endl;
 //t = solutionPtrs; //cout << t << endl;
// cout << "in parse_slew..." << s << endl;
 All_slew.insert( std::pair<string, double**>(s, t) );
 display_slew(fout, All_slew, Cload_vals, Tau_in_vals, s);
 fout.close();
return fin;
}

ostream& display_slew(ostream& fout, map<string, double**> All_slew, double Cload_vals[], double Tau_in_vals[], string s)
{
// fout << "cell: " << name << endl;
 fout << "input slews:\n";
 for(int i = 0; i < 6; i++)
 {
    fout << Tau_in_vals[i] << ',';
 }
 fout << Tau_in_vals[6] << endl;
 fout << "load caps:\n";
 for(int i = 0; i < 6; i++)
 {
    fout << Cload_vals[i] << ',';
 }
 fout << Cload_vals[6] << endl << endl;
 double** temp = All_slew[s];
fout << "slews:\n";//fout << (*iter).first << " is:\n";
for(int i = 0; i < 7; i++)
{
   for(int j = 0; j < 7; j++)
   {
      fout << std::fixed << std:: setprecision(8) << std::setw(8) << temp[i][j] << '\t';
   }  fout << endl;
}
 fout << endl << endl << endl << endl;
 return fout;
}

int main(int argc, char *argv[])
{
	string cmd, option;
	string input, output;
  output = "ckt_details.txt";
	ifstream fin;
	ofstream fout;
  string s;
  if(argc == 0 || argc == 1) 
  {
    cout << "no commands found...\n";
    return -1;
  }
	cmd = argv[1];
	std::remove( "delay_LUT.txt" );std::remove( "slew_LUT.txt" );std::remove( "ckt_details.txt" );
	if(cmd == "read_ckt")
	{
		input = argv[2];
  	node(input,output);
	}

	else if(cmd == "read_nldm")
	{

    if(argc != 4)
		{
			cout << "lack file input...\nprogram exit now...\n";
			return -1;
		}
		option = argv[2];
		input = argv[3];
		if(option == "delays")
		{
	 		fin.open(input.c_str());
      if(fin.fail())
      {
        cout << "error opening the file\n";
        exit(1);
      }
		 	while(!fin.eof())
			{
		    parse_delay(fin,s);
        if(fin.eof())
         break;
      }
	 		fin.close( );
		}

		else if(option == "slews")
		{
		 fin.open(input.c_str());
     if(fin.fail())
      {
        cout << "error opening the file\n";
        exit(1);
      }
		 while(!fin.eof())
		 {
		    parse_delay(fin,s);
        if(fin.eof())
         break;
  		 	parse_slew(fin,s);
        if(fin.eof())
         break;
		 }
	 	fin.close( );
		}

		else
		{
			cout << "invalid info to be parsed...\nprogram exit now...\n"; return -1;
		}
	}

  else if(argc == 2)  //this is phase 2
  {
    fin.open("sample_NLDM.lib.txt");

    if(fin.fail())
    {
      cout << "error opening the file\n";
      exit(1);
    }
    node(cmd, output); //cout << cmd << endl;
//    for(int i = 0; i < vin.size(); i ++) cout << vin[i] << endl;
     while(!fin.eof())
     {
        parse_delay(fin,s);
        parse_slew(fin,s);
     }
    setAllLoadCap();  //this function works 100%
    setinputSlew(vin);
    //setDelay(All_delay);
    while(!Q.empty())
    {//cout << "caaaa" << endl;
     cout << Q.front().ID  << " with load cap of " << Q.front().loadcap << endl;  //TODO:output_slew is 0 here
     for(int i = 0; i < Q.front().input_slew.size(); i ++)
       cout << Q.front().input[i].ID << ' ' << Q.front().input_slew[i] << '\t';
     cout << endl;
     Q.pop();
    }

    fin.close( );
  }
	else
	{
		cout << "no command found...\nprogram exit now...\n"; return -1;
	}

  	return 0;
}
