#include "session.h"
// {speed, duration, slope}

// MANUEL HIJO
Program  maeg1 = { // paseo
  .owner= "MaEG",
  .oseq = { {6,2,0}, {6.5,2,0}, {7,4,0}, //8 min
	    {6.5,3,0}, {7,4,0}, // 7 min
	    {6.5,5,0}, {6,5,0}, // 10 min
	    {0,0,0} //end token
  }
};

Program  maeg2 = {  // 30 min rapido
  .owner= "MaEG", 
  .oseq = { {7,1,0}, {8,3,0}, {9,3,0}, // 7 min
	    {10,3,0}, {8,2,0}, // 5 min
	    {9,3,0}, {10,5,0}, // 8 min
	    {9,1,0}, {11,4,0}, // 5 min
	    {9,3,0}, {8,1,0}, {7,1,0}, //5 min
	    {0,0,0} //end token
  }
};

Program  maeg3 = {  // 45 min resistencia
  .owner= "MaEG", 
  .oseq = { {6,5,0}, {7,10,0}, // 15 min
	    {8,5,0}, {7,5,0}, // 10 min
	    {8,5,0}, {9,5,0}, // 10 min
	    {8,3,0}, {7,5,0}, {6,2,0}, // 10 min
	    {0,0,0} //end token
  }
};



// MIGUEL 
Program  mieg1 = {
  .owner= "MiEG",
  .oseq = { {4,2,0}, {5,3,0}, {6,5,0}, {7,5,1},
	    {8,10,2}, {9,5,2}, {8,5,2},
	    {7,2,1}, {6,2,1}, {4,1,0},
	    {0,0,0} //end token
  }
};



// MARÍA JESÚS
Program  mjgg1 = {
  .owner= "MJGG",
  .oseq = { {3,1,0},{4,1,0}, {5,5,1}, // 7 min
	    {6,9,2}, // 9 min
	    {4,2,1}, {3,2,0}, // 4 min
	    {0,0,0} //end token
  }
};

Program  mjgg2 = {
  .owner= "MJGG",
  .oseq = { {3,1,0},{4,1,0}, {5,5,1}, // 7 min
	    {6,12,2}, // 12 min
	    {6.5, 10, 2}, //10
	    {5.5, 7, 2}, // 7
	    {4,2,1}, {3,2,0}, // 4 min
	    {0,0,0} //end token
  }
};

Program  mjgg3 = {
  .owner= "MJGG",
  .oseq = { {3,1,0},{4,1,0}, {5,5,1}, // 7 min
	    {6,20,2}, // 20 min
	    {6.5,15,2}, //15
	    {5.5,14,2}, // 14
	    {4,2,1}, {3,2,0}, // 4 min
	    {0,0,0} //end token
  }
};


/// MANUEL PADRE

Program  test = {
  .owner= "MEV",
  .oseq = {
    {4,1,0}, {6,1,0},
    {0,0,0} //end token
  }
};

Program  mev1 = { // 25 min
  .owner= "MEV",
  .oseq = {
    {4,1,0}, {5,1,0}, {6,3,0}, {7,5,1}, // 10 min warming
    {8.5,5,2}, {10,5,2}, // 10 min
    {7.5,3,1}, {6,1,0}, {4,1,0}, // 5 min   cooling
    {0,0,0} //end token
  }
};

Program  mev2 = { // 30 min
  .owner = "MEV",
  .oseq = {
    {4,1,0}, {5,1,0}, {6,3,0}, {7,5,1}, // 10 min warming
    {8,5,2}, {9,5,2}, {10,5,2}, // 15 min
    {7.5,3,1}, {6,1,0}, {4,1,0}, // 5 min   cooling
    {0,0,0} //end token
  }
};

Program  mev3 = { // 45 min
  .owner = "MEV",
  .oseq = {
    {4,1,0}, {5,1,0}, {6,3,0}, {7,5,1}, // 10 min warming
    {8,5,2}, {9,5,2},  {10,5,2}, // 15 min
    {9,5,2}, {10,5,2}, {8.5,5,2}, // 15 min
    {6,3,1}, {5,1,0}, {4,1,0}, // 5 min   cooling
    {0,0,0} //end token
  }
};

Program  mev4 = {  // 60 min
  .owner = "MEV",
  .oseq = {
    {4,1,0}, {5,1,0}, {6,3,0}, {7,5,1}, // 10 min warming
    {8,5,2}, {9,5,2}, {10,5,2}, // 15 min
    {9,5,2}, {10,5,2}, // 10 min
    {9,5,2}, {10,5,2},  // 10 min
    {9,5,2}, {7.5,5,2},  // 10 min
    {6,3,1}, {5,1,0}, {4,1,0}, // 5 min   cooling
    {0,0,0} //end token
  }
};

