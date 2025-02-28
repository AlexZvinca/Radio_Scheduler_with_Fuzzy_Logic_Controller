simple FLC
{
	parameters: 
		xml config= xmldoc("conf.xml");
	gates:
		input in;
}
 
 network FLC_Net
{
    submodules:
        flc: FLC {
            parameters:
          
                @display("p=89,100");
        }
        gen: GeneratorFLC {
            parameters:
                @display("p=209,100");
        }
  //      sink: Sink {
    //        parameters:
      //          @display("p=329,100");
        //}
    connections:
        gen.out --> flc.in;
    
}
 