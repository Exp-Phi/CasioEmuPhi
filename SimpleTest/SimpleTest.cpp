#include "pch.h"
#include "CppUnitTest.h"

#include "RomPackage.h"
#include <algorithm>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace SimpleTest
{
	inline void fillRandomData(unsigned char* buf, size_t size) {
		std::srand(static_cast<unsigned int>(clock())); // ʹ�õ�ǰʱ����Ϊ�������
		std::generate(buf, buf + size, []() {
			return static_cast<unsigned char>(std::rand() % 256); // ����0��255֮��������
			});
	}
	TEST_CLASS(SimpleTest)
	{
	public:

		TEST_METHOD(DecryptEncryptTest)
		{
			{
				RomPackage rp{};
				rp.Load("./test_in");
				rp.Encrypt("wssyd");
				WriteFile("test.package", rp);
			}
			{
				RomPackage rp{};
				ReadFile("test.package", rp);
				rp.Decrypt("wssyd");
				rp.ExtractTo("./extracted");
			}
		}
	};
}
