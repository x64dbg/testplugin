void main()
{
    // Print the value that we received
    Print("Script started!\n");
    
    // Do some stepping
    for(int i=0; i<100; i++) {
    	Print("Stepping " + (i+1) + "...\n");
    	StepIn();
    }
    StepOver();
    StepOut();
}
