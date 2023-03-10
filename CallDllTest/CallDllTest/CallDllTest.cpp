// CallDllTest.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <Windows.h>
#include <tchar.h>

int main()
{

	HINSTANCE handle = LoadLibrary(_T("Dll_study.dll"));//LoadLibrary ddl ,需要引入<tchar.h> 加上_T，不然编译报错
	std::cout << "dll的句柄返回值:" << handle << "\n";//打印dll的句柄地址值,不为0就说明调用成功
	if (handle)
	{
		typedef int(*DLL_FUNCTION_ADD) (int, int); //typedef定义一下函数指针，(int, int) 是要调用dll中的函数参数类型
		DLL_FUNCTION_ADD dll_add = (DLL_FUNCTION_ADD)GetProcAddress(handle, "my_add"); //使用GetProcAddress得到dll中的函数，重命名为 dll_add
		std::cout << "dll 函数的句柄返回值:" << handle << "\n";//打印dll的函数句柄地址值,不为0就说明调用成功
		if (dll_add)
		{
			int result = dll_add(10, 11); //终于可以用add了
			std::cout << "dll_add结算结果:" << result << "\n";
			FreeLibrary(handle); //卸载句柄，，
		}
	}
	std::cout << "Hello World!\n";
	return 0;
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
