#include "jsonform.h"

using namespace std;

map<unsigned int, OCube> cubegraph;

string FormJSON(map<unsigned int, OCube> &cubegraph) {
	string out = "[";
	map<unsigned int, OCube>::iterator bg = cubegraph.begin();
	vector<OCubeState>::iterator bg1, en1;
	vector<unsigned int>::iterator bg2, en2;
	for (; bg != cubegraph.end(); ++bg) {
		out = out + "{\"name\":" + to_string(bg->first) + ",\"type\":\"" + bg->second.type + "\",\"state\":[";
		bg1 = bg->second.params.begin();
		en1 = bg->second.params.end();
		for (; bg1 != en1; ++bg1) {
			out += "{\"vtype\":\"" + bg1->vtype + "\",\"value\":" + to_string(bg1->value) + "},";
		}
		if (bg1 != en1) out.resize(out.length() - 1);
		out += "],\"relations\":[";
		bg2 = bg->second.relations.begin();
		en2 = bg->second.relations.end();
		for (; bg2 != en2; ++bg2) {
			out += *bg2 ? to_string(*bg2) + "," : "\"OUT\",";
		}
		if (bg2 != en2) out.resize(out.length() - 1);
		out += "]},";
	}
	if (bg != cubegraph.end()) out.resize(out.length() - 1);
	return out + "]";
}

int main()
{
	vector<unsigned int> vec1;
	vector<OCubeState> vec2;
	vec1.push_back(0);
	vec2.push_back(OCubeState("low-clip", 1.8));
	vec2.push_back(OCubeState("high-clip", 18.4));
	cubegraph.insert(pair<unsigned int, OCube>(1, OCube("filter", vec1, vec2)));
	vec2.clear();
	vec1.clear();
	vec2.push_back(OCubeState("freq", 8000));
	vec2.push_back(OCubeState("volume", 50));
	vec1.push_back(1);
	cubegraph.insert(pair<unsigned int, OCube>(2, OCube("sine-gen", vec1, vec2)));
	cout << FormJSON(cubegraph)  << endl;
	system("pause");
}
