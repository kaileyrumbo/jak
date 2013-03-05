// jak.cpp

/* TODO: Create input file format
reps= 10						
branches= ((A:0.1,B:0.1):0.7,C:0.5);			//quoted labels and keys or relabel w/ perl
thetas= ((A:0.5,B:0.5):0.4,C:0.6):0.8;
A = 10
B = 5
C = 5
seed = 1776
*/

#ifdef _MSC_VER
#	include <process.h>
#	define getpid _getpid
#else
#	include <unistd.h>
#endif

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <ctime>
#include <vector>
#include <string>
#include <sstream>
#include <limits>
#include <cmath>
#include <cfloat>
#include <fstream>
#include <stdio.h>

#include "xorshift64.h"
#include "rexp.h"
#include "aliastable.h"

using namespace std;

// Conditional probably of mutation
double mutation_matrix[4][4] = {
/*A*/ {0.25, 0.25, 0.25, 0.25},
/*C*/ {0.25, 0.25, 0.25, 0.25},
/*G*/ {0.25, 0.25, 0.25, 0.25},
/*T*/ {0.25, 0.25, 0.25, 0.25}
};
alias_table mutation[4];

// data structure to hold node information
struct nodestruct {
    int child_1;   // ID of right child
    int child_2;   // ID of left child
    int parent;    // ID of parent
    double time;   // "distance" to parent
    int species;   // species/population identifier
    char label;    // nucleotide
	
	nodestruct() : child_1(-1), child_2(-1), parent(-1),
	               time(0.0), species(0), label(0) { }
};

void coaltree(xorshift64& myrand1, vector<int>& activelist, double theta, double time, char species,
	          vector<nodestruct>& nodeVector);
int set_mutations(xorshift64 &myrand1, char &G, double time);

string id_to_string(int x);
string species_label(int type);
string tree_to_string(const vector<nodestruct>& v);
string mutation_string(const vector<nodestruct>& t);

//-----------------------------------------------------------------------------//
// random seed generator
inline unsigned int create_random_seed() {
    unsigned int v = static_cast<unsigned int>(getpid());
    v += ((v << 15) + (v >> 3)) + 0x6ba658b3; // Spread 5-decimal PID over 32-bit number
    v^=(v<<17);
    v^=(v>>13);
    v^=(v<<5);
    v += static_cast<unsigned int>(time(NULL));
    v^=(v<<17);
    v^=(v>>13);
    v^=(v<<5);
    return (v == 0) ? 0x6a27d958 : (v & 0x7FFFFFFF); // return at most a 31-bit seed
}

//-------------------------------------------------------------------------------------------------//

int main(int argc, char *argv[])
{
    int N1, N2, n, trees;
    double theta1, theta2, theta3, t1, t2;

	//TODO: fix input validation
    if (argc == 9) {
        trees=atoi(argv[1]);
		while(trees<1)
		{	cout<<"Error: Enter number of trees: ";
			cin>>trees;
		}
		N1=atoi(argv[2]);
		while(N1<1)
		{	cout<<"Error: Enter number of tips for first species: ";
			cin>>N1;
		}
        N2=atoi(argv[3]);
		while(N2<1)
		{	cout<<"Error: Enter number of tips for second species: ";
			cin>>N1;
		}
        theta1=strtod(argv[4], NULL);
		while(theta1<0.0)
		{	cout<<"Error: Enter theta1: ";
			cin>>theta1;
		}
        theta2=strtod(argv[5], NULL);
		while(theta2<0.0)
		{	cout<<"Error: Enter theta2: ";
			cin>>theta2;
		}
        theta3=strtod(argv[6], NULL);
		while(theta3<0.0)
		{	cout<<"Error: Enter theta3: ";
			cin>>theta3;
		}
        t1=strtod(argv[7], NULL);
		while(t1<0.0)
		{	cout<<"Error: Enter t1: ";
			cin>>t1;
		}
        t2=strtod(argv[8], NULL);
		while(t2<0.0)
		{	cout<<"Error: Enter t2: ";
			cin>>t2;
		}
    }
    else if (argc == 1) {
        cout << "Enter number of trees: ";
        cin >> trees;
        cout << endl << "Enter the number of tips for first species: ";
        cin >> N1;                                                                             //receive # of starting nodes (species 1)
        cout << endl << "Enter number of tips for second species: ";
        cin >> N2;                                                                             //receive # of starting nodes (species 2)
        cout << endl << "Enter theta 1: ";
        cin >> theta1;
        cout << endl << "Enter theta 2: ";
        cin >> theta2;
        cout << endl << "Enter theta 3: ";
        cin >> theta3;
        cout << endl << "Enter t1: ";
        cin >> t1;
        cout << endl << "Enter t2: ";
        cin >> t2;
    } else {
        cerr << "error, must have format: prg name, trees, # of tips for first"
                "species, # of tips for second species, theta1, theta2,"
                "theta3, t1, t2" << endl;
		cerr << "Press ENTER to quit." << flush;
		cin.clear();
		cin.sync();
		cin.ignore( numeric_limits<streamsize>::max(), '\n' );
        return EXIT_FAILURE;
    }

    n = N1+N2; //n=total original tips from both species

	//use xorshift64 class for random number generator
    xorshift64 myrand;
    myrand.seed(create_random_seed());

	// construct alias tables for mutation simulation
	for(int i=0;i<4;++i) {
		mutation[i].create(&mutation_matrix[i][0],&mutation_matrix[i][4]);
	}
	
	// loop once for each tree
    for(int repeat=0; repeat<trees; repeat++)
	{ 
        // create nodevector (vector of structs)
		vector<nodestruct> nodevector(n);
		
		// initialize active list for species 1
		vector<int> active1(N1); 
		for(int i=0; i<N1; i++)
		{
			active1[i]=i;
			nodevector[i].species=1;
		}

		// initialize active list for species 2
		vector<int> active2(N2);
		for(int j=N1; j<N1+N2; j++)
		{
			active2[j-N1]=j;
			nodevector[j].species=2;
		}
		
		// coalesce population 1
		coaltree(myrand, active1, theta1, t1, 1, nodevector);
		
		// coalesce population 2
		coaltree(myrand, active2, theta2, t2, 2, nodevector);

		// coalesce population 3
        active1.insert(active1.end(), active2.begin(), active2.end());
        coaltree(myrand, active1, theta3, DBL_MAX, 3, nodevector);

		// Assume root has nucleotide 'A'
        nodevector.back().label = 0;
		// start mutations for loop
        for (int i = (int)nodevector.size() - 2; i >= 0; --i)
		{                                               
            nodevector[i].label = nodevector[nodevector[i].parent].label;
            set_mutations(myrand, nodevector[i].label, nodevector[i].time);
        }
		// Label nodes
        for (size_t i=0; i < nodevector.size(); i++){
            char s[] = "ACGT";
            nodevector[i].label = s[(size_t)nodevector[i].label];
        }

//----------------------------------------------------------------------------//

		cout << mutation_string(nodevector) << "\t";
        cout << tree_to_string(nodevector) << endl; //print newick tree to console
    } //end # of trees loop

#ifndef NDEBUG
    cerr<<"Random seed used: "<<create_random_seed()<<endl;
    cerr << "Press ENTER to quit." << flush;
	cin.clear();
	cin.sync();
    cin.ignore( numeric_limits<streamsize>::max(), '\n' );
#endif
    return EXIT_SUCCESS;
}

void coaltree(xorshift64& myrand1, vector<int>& activelist, double theta, double time, char species,
	          vector<nodestruct>& nodeVector)
{
    double T = 0.0;
	int random1, random2;

	int size = (int)activelist.size();

	while(size>1)
	{
        // Draw waiting time until next coalescent event
		double mean = (2.0/(size*(size-1.0)))*(theta/2.0);
		T += rand_exp(myrand1, mean);
		if(T > time)
			break;

		// pick a random pair of nodes
		random1 = static_cast<int>(size*myrand1.get_double52());
		random2 = static_cast<int>((size-1)*myrand1.get_double52());
		random2 = (random1+random2+1) % size;
		if(random1 > random2)
			swap(random1,random2);
		
		int newparent = (int)nodeVector.size();
		nodeVector.push_back(nodestruct());
		
		nodeVector[newparent].species = species;

		//update parent node
		nodeVector[newparent].child_1 = activelist[random1];
		nodeVector[newparent].child_2 = activelist[random2];
		nodeVector[newparent].time = T;
		
		//update child nodes
		nodeVector[activelist[random1]].parent = newparent;
		nodeVector[activelist[random2]].parent = newparent;
		nodeVector[activelist[random1]].time = T - nodeVector[activelist[random1]].time;
		nodeVector[activelist[random2]].time = T - nodeVector[activelist[random2]].time;

		//update active vector
		activelist[random1] = newparent;
		activelist.erase (activelist.begin() + random2);

		size--;
	}

	for(int i=0; i<size; i++)
		nodeVector[activelist[i]].time = nodeVector[activelist[i]].time - time;
}

// Function to convert int type to string
// Cache labels for reuse
string id_to_string(int x) {   
	static vector<string> v;
	static char buffer[64];
	for(int xx = (int)v.size(); xx <= x; ++xx) {
		sprintf(buffer, "%d", xx);
		v.push_back(buffer);
	}
    return v[x];
}

// TODO: Cache this
//Function to convert species number to letter format for tree output
string species_label(int species)
{
	// Species numbers start at 1.
	assert(species >= 1);
	static vector<string> v;
	species -= 1;
	if(species < 0)
		return "%";
	string ans;
	int zz = 0;
	int aa = 0;
	
	for(int xx=(int)v.size(); xx <= species; ++xx) {
		ans.clear();
		zz = xx;
		
		if (zz <= 25)
			ans += 'A' + zz;
			
		else {
			while(zz >= 26){
			zz = zz - ans.size();
			ans += 'A' + zz%26;
			zz /= 26;
			aa = 1;
			if (zz == 26)
				break;
			} 
			if (aa == 1)
				zz = zz - 1;
			ans += 'A' + zz%26;
		}
		
		reverse(ans.begin(),ans.end());
		v.push_back(ans);
	}
    return v[species];
}
//-----------------------------------------------------------------------------//
//create newick tree from node data
string tree_to_string(const vector<nodestruct>& v) {
    vector<string> node_str(v.size(),"");
    char buffer[16];
    for(size_t i=0; i<v.size(); i++) {
        string temp = "";

        if(v[i].child_1 != -1 && v[i].child_2 != -1) {
            string convert_node1=id_to_string(v[i].child_1);
            string convert_node2=id_to_string(v[i].child_2);
			temp += "(" + node_str[v[i].child_1] + species_label(v[v[i].child_1].species) + convert_node1 + ":";
            sprintf(buffer, "%0.6g", v[v[i].child_1].time);
			temp += buffer;
            temp += "," + node_str[v[i].child_2] + species_label(v[v[i].child_2].species) + convert_node2+ ":";
            sprintf(buffer, "%0.6g", v[v[i].child_2].time);
            temp += buffer;
			temp += ")";
        }
        node_str[i] = temp;
    }
	string temp = species_label(v.back().species) + id_to_string((int)v.size() - 1);
    return node_str.back() + temp + ";";
}
//-----------------------------------------------------------------------------//
// Function to construct vector of mutation labels for tree, in numerical order
string mutation_string(const vector<nodestruct>& t)
{
	string temp = "[";
	for(size_t i=0; i<t.size(); i++) {
		temp += t[i].label;
	}
	temp += ']';
	return temp;
}

//-----------------------------------------------------------------------------//
int set_mutations(xorshift64 &myrand1, char &G, double time)
{   
    int counter = 0;
	double m = rand_exp(myrand1); //m = total distance travelled along branch lengthf
    while (m <= time) { //if m < branch length --> mutate
        ++counter;  //muation counter
		// use the alias tables to effeciently sample the result of the mutation
		G = static_cast<char>(mutation[(size_t)G](myrand1.get_uint64()));
        m += rand_exp(myrand1);
    }
	return counter;
}
