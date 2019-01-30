#include "jsonform.h"
#include <iostream>

using namespace std;

map<unsigned int, OCube> cubegraph;

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
