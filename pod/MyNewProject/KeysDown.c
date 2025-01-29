uint8_t numKeysDown = 0;
const uint8_t MAX_KEYS_DOWN = 8;


Note k = {pop note from midi queue}.AsNote(m);


void NoteOff(k)
{

    NoteOffEvent p = m.AsNoteOff();
    hw.seed.Print("NoteOff: Ch=%d, Note=%d, Vel=%d\r\n", p.channel, p.note, p.velocity);

//    uint8_t i=0;
//    while( i < numKeysDown ){
//        check
//        if match, remove, numKeysDown--
//            then shift next slot up, repeat
//    }

//    osc.SetAmp((0));

}

void NoteOn(k)
{
    if(k.vel = 0){
        NoteOff(k);
        return;
    }
    
    uint8_t temp = 0;


    for( uint8_t i=0; i<MAX_KEYS_DOWN; ++i) {

        if( keysDown[i].note == 0 ) {
            keysDown[i].note = k.note;
            numKeysDown++;
            return;
        }

        if( k.note < keysDown[i].note ) {
            continue;
        }

        temp = keysDown[i].note; 
        keysDown[i].note = k.note;
        numKeysDown++;
        while( i++ < numKeysDown) {
            keysDown[i].note = temp;
            temp = keysDown[i+1].note;
        }
        return;

    }



    if(numKeysDown == MAX_KEYS_DOWN){
        keysDown[numKeysDown-1]
        
        // delete oldest/lowest note
        numKeysDown--;
    }



    if(numKeysDown == 0){
        // put new in slot 0
    v

