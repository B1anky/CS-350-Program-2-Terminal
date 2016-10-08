Name: Brett Sackstein

To compile, call make. To run, <./cs350sh>. I've tested quite a few test cases and
have not received a segmentation fault or a memory map error, so the code
is pretty clean. I believe all test cases pass with my code in both foreground and background. All combinations of <, >, and | work as intended.
For example: 
wc -l < input.txt | ls -l | wc -l > output.txt &
Will be a valid command that executes correctly with the same output as the normal terminal to output.txt. 
