#include<vector>
#include<functional>
#include<iostream>

using namespace std;

int main()
{
	vector<function<void(void)>> commandlist;

	commandlist.push_back([]() {cout << "GPU Set" << endl;});//void anonymous_function() {cout << "GPU Set RTV-1" << endl;}‚ð‚Ps‚Å
	cout << "RTV-2" << endl;

	commandlist.push_back([]() {cout << "GPU clear" << endl;});
	cout << "RTV-4" << endl;

	commandlist.push_back([]() {cout << "GPU Close" << endl;});
	cout << "RTV-6" << endl;

	cout << endl;

	for (auto& cmd : commandlist)
	{
		cmd();
	}

	getchar();

	return 0;
}

