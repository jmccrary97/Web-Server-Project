
// ***********************************************************
// ** If you want to define other functions, put them here.
// ***********************************************************
void A_init();
void B_init();

bool rdt_sendA(struct msg message);
bool rdt_sendB(struct msg message);  /* You should leave this empy */

void rdt_rcvA(struct pkt packet);
void rdt_rcvB(struct pkt packet);

void A_timeout();
void B_timeout();

// ***********************************************************
// ** Simple operator functions to make output look cleaner.
// ***********************************************************
std::ostream& operator<<(std::ostream& os, const struct msg& message);
std::ostream& operator<<(std::ostream& os, const struct pkt& packet);

// ***********************************************************
// ** The simulator is a global.
// ***********************************************************
extern simulator *simulation;
