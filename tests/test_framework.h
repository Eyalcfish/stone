#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include <chrono>
#include <sstream>

namespace TestFramework {

struct TestCase {
	std::string name;
	std::function<void()> func;
};

inline std::vector<TestCase>& getTestRegistry() {
	static std::vector<TestCase> registry;
	return registry;
}

inline void registerTest(const std::string& name, std::function<void()> func) {
	getTestRegistry().push_back({name, func});
}

struct TestContext {
	int assertionsCount = 0;
	int failuresCount = 0;
	std::string currentTestName;
	std::vector<std::string> failureMessages;
};

inline TestContext& getContext() {
	static TestContext ctx;
	return ctx;
}

#define TEST_CONCAT_INNER(a, b) a ## b
#define TEST_CONCAT(a, b) TEST_CONCAT_INNER(a, b)

#define TEST_CASE(name) \
	static void TEST_CONCAT(test_func_, __LINE__)(); \
	namespace { \
		struct TEST_CONCAT(TestAutoReg_, __LINE__) { \
			TEST_CONCAT(TestAutoReg_, __LINE__)() { \
				::TestFramework::registerTest(name, TEST_CONCAT(test_func_, __LINE__)); \
			} \
		} TEST_CONCAT(test_auto_reg_, __LINE__); \
	} \
	static void TEST_CONCAT(test_func_, __LINE__)()

#define ASSERT_TRUE(expr) \
	do { \
		::TestFramework::getContext().assertionsCount++; \
		if (!(expr)) { \
			::TestFramework::getContext().failuresCount++; \
			std::ostringstream oss; \
			oss << "ASSERT_TRUE failed: " << #expr << " at " << __FILE__ << ":" << __LINE__; \
			::TestFramework::getContext().failureMessages.push_back(oss.str()); \
		} \
	} while (0)

#define ASSERT_FALSE(expr) \
	do { \
		::TestFramework::getContext().assertionsCount++; \
		if (expr) { \
			::TestFramework::getContext().failuresCount++; \
			std::ostringstream oss; \
			oss << "ASSERT_FALSE failed: " << #expr << " at " << __FILE__ << ":" << __LINE__; \
			::TestFramework::getContext().failureMessages.push_back(oss.str()); \
		} \
	} while (0)

#define ASSERT_EQ(a, b) \
	do { \
		::TestFramework::getContext().assertionsCount++; \
		if (!((a) == (b))) { \
			::TestFramework::getContext().failuresCount++; \
			std::ostringstream oss; \
			oss << "ASSERT_EQ failed: " << #a << " (" << (a) << ") == " << #b << " (" << (b) << ") at " << __FILE__ << ":" << __LINE__; \
			::TestFramework::getContext().failureMessages.push_back(oss.str()); \
		} \
	} while (0)

#define ASSERT_NE(a, b) \
	do { \
		::TestFramework::getContext().assertionsCount++; \
		if ((a) == (b)) { \
			::TestFramework::getContext().failuresCount++; \
			std::ostringstream oss; \
			oss << "ASSERT_NE failed: " << #a << " (" << (a) << ") != " << #b << " (" << (b) << ") at " << __FILE__ << ":" << __LINE__; \
			::TestFramework::getContext().failureMessages.push_back(oss.str()); \
		} \
	} while (0)

inline int runAllTests() {
	auto& tests = getTestRegistry();
	int totalPassed = 0;
	int totalFailed = 0;

	std::cout << "==================================================" << std::endl;
	std::cout << "Running " << tests.size() << " test cases..." << std::endl;
	std::cout << "==================================================" << std::endl;

	auto startTime = std::chrono::high_resolution_clock::now();

	for (const auto& test : tests) {
		TestContext& ctx = getContext();
		ctx.currentTestName = test.name;
		ctx.failureMessages.clear();
		int prevFailures = ctx.failuresCount;

		auto t0 = std::chrono::high_resolution_clock::now();
		try {
			test.func();
		} catch (const std::exception& e) {
			ctx.failuresCount++;
			ctx.failureMessages.push_back(std::string("Exception caught: ") + e.what());
		} catch (...) {
			ctx.failuresCount++;
			ctx.failureMessages.push_back("Unknown non-std exception caught");
		}
		auto t1 = std::chrono::high_resolution_clock::now();
		auto durationUs = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

		if (ctx.failuresCount == prevFailures) {
			totalPassed++;
			std::cout << "[ PASS ] " << test.name << " (" << durationUs << " us)" << std::endl;
		} else {
			totalFailed++;
			std::cout << "[ FAIL ] " << test.name << " (" << durationUs << " us)" << std::endl;
			for (const auto& msg : ctx.failureMessages) {
				std::cout << "         -> " << msg << std::endl;
			}
		}
	}

	auto endTime = std::chrono::high_resolution_clock::now();
	auto totalDurationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

	std::cout << "==================================================" << std::endl;
	std::cout << "Test Summary:" << std::endl;
	std::cout << "  Total Tests: " << tests.size() << std::endl;
	std::cout << "  Passed:      " << totalPassed << std::endl;
	std::cout << "  Failed:      " << totalFailed << std::endl;
	std::cout << "  Assertions:  " << getContext().assertionsCount << std::endl;
	std::cout << "  Duration:    " << totalDurationMs << " ms" << std::endl;
	std::cout << "==================================================" << std::endl;

	return (totalFailed == 0) ? 0 : 1;
}

} // namespace TestFramework
