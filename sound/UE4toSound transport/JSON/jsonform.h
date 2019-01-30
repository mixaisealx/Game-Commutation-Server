#include <string>
#include <vector>
#include <map>

struct OCubeState {
	string vtype;
	double value;
	OCubeState(string vtype, double value): vtype(vtype), value(value) {}
};

struct OCube {
	vector<unsigned int> relations;
	vector<OCubeState> params;
	string type;
	OCube(string type, vector<unsigned int> relations, vector<OCubeState> params): type(type), relations(relations), params(params) {}
};

string FormJSON(map<unsigned int, OCube> &cubegraph);