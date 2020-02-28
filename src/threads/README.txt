Jazz Loffredo
CIS 520
Proj0

Changes Required (+ = New File Added) (~ = Edited Existing File):
+ src/tests/threads/log-multiple.txt
+ src/tests/threads/log-mega.txt
+ src/tests/threads/alarm-mega.ck
~ src/tests/threads/alarm-wait.c
~ src/tests/threads/tests.c
~ src/tests/threads/tests.h
~ src/tests/threads/Make.tests


Description of Files:
- log-multiple.txt
	- This is a log file of the output from running alarm-multiple.
- log-mega.txt
	- This is a log file of the output from running alarm-mega.
- alarm-mega.ck
	- This file was copied from alarm-multiple.ck. I modified the parameter 
	  to check_alarm to be "70" so that is was consistent with the number of 
	  expected alarms for the alarm-mega test case. The purpose of this file 
	  is to verify that the correct number of lines are output for the alarm-mega 
	  test case using the check_alarm function which asserts pass on valid output.
- alarm-wait.c
	- This file is where the function for test_alarm_mega is stored. This function
	  calls an existing function, test_sleep, which takes parameters for thread
	  count and number of iterations. For the purposes of this assignment, I simply
	  modified the test_alarm_multiple function slightly so that, in the new mega function,
	  the parameters to test_sleep are (5, 70) so that 5 threads will sleep 70 times each.
- tests.c
	- In this file, I made an addition to the tests[] array to include a reference
	  to the new function in alarm-wait.c called test_alarm_mega. Similar naming
	  conventions were followed in accordance with the other functions. This file
	  is responsible for running the tests through the run_test function through
	  the use of function pointers.
- tests.h
	- This is the header file for tests.c. I added a line that declares the function
	  test_alarm_mega as an externally referenced function. Therefore, when test_alarm_mega
	  is used in tests.c, the compiler understands that it is referenced externally
	  in another file - specifcally our alarm-wait.c file.
- Make.tests
	- This is a typical Make file and the only modification was to add a reference to
	  the name "alarm-mega" in the tests/threads_TESTS variable. No other changes
	  were made to this file.
	   