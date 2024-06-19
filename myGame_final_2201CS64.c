#include <stdlib.h>
#include<stdio.h>
#include <unistd.h>
#include <time.h>
#define BOARD                 "DE1-SoC"

#define PUSHBUTTONS ((volatile long *)0xFF200050)
/* Memory */
#define DDR_BASE              0x00000000
#define DDR_END               0x3FFFFFFF
#define A9_ONCHIP_BASE        0xFFFF0000
#define A9_ONCHIP_END         0xFFFFFFFF
#define SDRAM_BASE            0xC0000000
#define SDRAM_END             0xC3FFFFFF
#define FPGA_ONCHIP_BASE      0xC8000000
#define FPGA_ONCHIP_END       0xC803FFFF
#define FPGA_CHAR_BASE        0xC9000000
#define FPGA_CHAR_END         0xC9001FFF

/* Cyclone V FPGA devices */
#define LEDR_BASE             0xFF200000
#define HEX3_HEX0_BASE        0xFF200020
#define HEX5_HEX4_BASE        0xFF200030
#define SW_BASE               0xFF200040
#define KEY_BASE              0xFF200050
#define JP1_BASE              0xFF200060
#define JP2_BASE              0xFF200070
#define PS2_BASE              0xFF200100
#define PS2_DUAL_BASE         0xFF200108
#define JTAG_UART_BASE        0xFF201000
#define JTAG_UART_2_BASE      0xFF201008
#define IrDA_BASE             0xFF201020
#define TIMER_BASE            0xFF202000
#define AV_CONFIG_BASE        0xFF203000
#define PIXEL_BUF_CTRL_BASE   0xFF203020
#define CHAR_BUF_CTRL_BASE    0xFF203030
#define AUDIO_BASE            0xFF203040
#define VIDEO_IN_BASE         0xFF203060
#define ADC_BASE              0xFF204000

/* Cyclone V HPS devices */
#define HPS_GPIO1_BASE        0xFF709000
#define HPS_TIMER0_BASE       0xFFC08000
#define HPS_TIMER1_BASE       0xFFC09000
#define HPS_TIMER2_BASE       0xFFD00000
#define HPS_TIMER3_BASE       0xFFD01000
#define FPGA_BRIDGE           0xFFD0501C

/* ARM A9 MPCORE devices */
#define   PERIPH_BASE         0xFFFEC000    // base address of peripheral devices
#define   MPCORE_PRIV_TIMER   0xFFFEC600    // PERIPH_BASE + 0x0600

/* Interrupt controller (GIC) CPU interface(s) */
#define MPCORE_GIC_CPUIF      0xFFFEC100    // PERIPH_BASE + 0x100
#define ICCICR                0x00          // offset to CPU interface control reg
#define ICCPMR                0x04          // offset to interrupt priority mask reg
#define ICCIAR                0x0C          // offset to interrupt acknowledge reg
#define ICCEOIR               0x10          // offset to end of interrupt reg
/* Interrupt controller (GIC) distributor interface(s) */
#define MPCORE_GIC_DIST       0xFFFED000    // PERIPH_BASE + 0x1000
#define ICDDCR                0x00          // offset to distributor control reg
#define ICDISER               0x100         // offset to interrupt set-enable regs
#define ICDICER               0x180         // offset to interrupt clear-enable regs
#define ICDIPTR               0x800         // offset to interrupt processor targets regs
#define ICDICFR               0xC00         // offset to interrupt configuration regs

// /* set a single pixel on the screen at x,y
//  * x in [0,319], y in [0,239], and colour in [0,65535]
//  */
void write_pixel(int x, int y, short colour) {
  volatile short *vga_addr=(volatile short*)(0x08000000 + (y<<10) + (x<<1));
  *vga_addr=colour;
}
int exit_game=0;
/* use write_pixel to set entire screen to black (does not clear the character buffer) */
void clear_screen() {
  int x, y;
  for (x = 0; x < 320; x++) {
    for (y = 0; y < 240; y++) {
	  write_pixel(x,y,0);
	}
  }
}
int score=0;
int car_x;
int car_y;
int level=1;
long long colour[240][320];
long long  prev_colour[240][320];
long long  pp_colour[240][320];
void write_char(int x, int y, char c) {
  // VGA character buffer
  volatile char * character_buffer = (char *) (0x09000000 + (y<<7) + x);
  *character_buffer = c;
}
void set_border(){
    int up=0,down=0,left=0,right=0;
    //up
    for(up=0;up<320;up++){
        write_pixel(up,0,0xFFFF00);
        write_pixel(up,1,0xFFFF00);
        write_pixel(up,2,0xFFFF00);
    }
    //down
    for(down=0;down<320;down++){
        write_pixel(down,237,0xFFFF00);
        write_pixel(down,238,0xFFFF00);
        write_pixel(down,239,0xFFFF00);
    }
    //left
    for(left=0;left<240;left++){
        write_pixel(0,left,0xFFFF00);
        write_pixel(1,left,0xFFFF00);
        write_pixel(2,left,0xFFFF00);
    }
    //right
    for(right=0;right<240;right++){
         write_pixel(317,right,0xFFFF00);
        write_pixel(318,right,0xFFFF00);
        write_pixel(319,right,0xFFFF00);
    }
    //tracks
    int track=0;
    for(track=0;track<215;track++){
        write_pixel(104,track,0xFFFF00);
        write_pixel(105,track,0xFFFF00);
        write_pixel(106,track,0xFFFF00);
        write_pixel(211,track,0xFFFF00);
        write_pixel(212,track,0xFFFF00);
        write_pixel(213,track,0xFFFF00);
    }
    //bottom part
    int bottom=0;
    for(bottom=0;bottom<319;bottom++){
       write_pixel(bottom,215,0xFFFF00);
       write_pixel(bottom,216,0xFFFF00);
       write_pixel(bottom,217,0xFFFF00);
    }
}
//define attributes of the block
typedef struct Block
{
    int x;
    int y;
    int size;
    int exists;
    int speed;
} block;
const int number_blocks=6;
int  car_clr=0x00ab41;
block block_list[9] = {{0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0},{0, 0, 0, 0, 0},{0, 0, 0, 0, 0},{0, 0, 0, 0, 0}};
int color_codes[9] = {
        0x00FFFF,  // Cyan
        0x00FFFF,  // Cyan
        0xFF00FF,   // Magenta
        0x0000FF,  // Blue
        0xFFA500,  // Orange
        0x00FF80,   // Turquoise
        0x800080,  // Purple
        0x8B0000,  // Pink
        0x32CD32  //Lime
    };
int val;
//this is to check input value and move our block accordingly
void check_button(){
   if(val==1){ //right
    if(car_x==53){
        val=0;
        car_x=160;
        for(int i=53-10;i<53+10;i++){
            for(int y=200;y<215;y++){
                colour[i][y]=0;
                prev_colour[i][y]=0;
                pp_colour[i][y]=0;
                write_pixel(i,y,0);
                if(colour[i+107][y]==0){
                    write_pixel(i+107,y,car_clr);
                colour[i+107][y]=car_clr;
                prev_colour[i+107][y]=0;
                pp_colour[i+107][y]=0;
                }
            }
        }
        return;
    }
    else if(car_x==160 && val==1){
        val=0;
        car_x=267;
        for(int i=160-10;i<160+10;i++){
            for(int y=200;y<215;y++){
                colour[i][y]=0;
                prev_colour[i][y]=0;
                pp_colour[i][y]=0;
                write_pixel(i,y,0);
                if(colour[i+107][y]==0){
                    write_pixel(i+107,y,car_clr);
                colour[i+107][y]=car_clr;
                prev_colour[i+107][y]=0;
                pp_colour[i+107][y]=0;
                }
            }
        }
        return;
    }
    
   }
   else if(val==2){
    if(car_x==160){
        car_x=53;
         val=0;
        for(int i=160-10;i<160+10;i++){
            for(int y=200;y<215;y++){
                colour[i][y]=0;
                prev_colour[i][y]=0;
                pp_colour[i][y]=0;
                write_pixel(i,y,0);
                if(colour[i-107][y]==0){
                    write_pixel(i-107,y,car_clr);
                colour[i-107][y]=car_clr;
                prev_colour[i-107][y]=0;
                pp_colour[i-107][y]=0;
                }
            }
        }
        return;
    }
    else if(car_x==267 && val==2){
        car_x=160;
        val=0;
        for(int i=267-10;i<267+10;i++){
            for(int y=200;y<215;y++){
                colour[i][y]=0;
                prev_colour[i][y]=0;
                pp_colour[i][y]=0;
                write_pixel(i,y,0);
                if(colour[i-107][y]==0){
                    write_pixel(i-107,y,car_clr);
                colour[i-107][y]=car_clr;
                prev_colour[i-107][y]=0;
                pp_colour[i-107][y]=0;
                }
            }
        }
        return;
    }
   }
}
//Function to clear a character at any specified location (x,y)
void clear_char() {
	for (int x = 0; x<80; x++){
		for (int y=0; y<60; y++){
  			volatile char * character_buffer = (char *) (0x09000000 + (y<<7) + x);
  			*character_buffer = '\0';
		}
	}

}
//to print the result of the game
void print_result(){
    clear_screen();
    clear_char();
    char* hw = "GAME OVER";
    int x=35;
   while (*hw) {
     write_char(x, 25, *hw);
	 x++;
	 hw++;
   }
  hw = "Score:";
    x=35;
   while (*hw) {
     write_char(x, 28, *hw);
	 x++;
	 hw++;
   }
 char str[5];
   //a functional way to convert num to string
   sprintf(str, "%d", score);
    int a=0;
   x=42;
    while (a<5) {
    write_char(x, 28, str[a]);
    x++;
    a++;
   }

}
//to check that when i shift my block on another track then it hit or not
void check_collide(){
     for(int i=0;i<number_blocks;i++){
        if((prev_colour[block_list[i].x][block_list[i].y+2*block_list[i].size-2]==car_clr)){
            exit_game=1;
            print_result();
        }
     }
}
//to check if any incoming block hits my block
void check_collision(int x,int y){
    if(colour[x][y]==car_clr || prev_colour[x][y]==car_clr){
        exit_game=1;
        print_result();
    }
}
//to draw the new block
void generate(int x,int size,int num){
    int clr=rand()%number_blocks;
    // block_list[num]
    for(int i=x-size;i<x+size;i++){
            for(int j=3;j<2*size+3;j++){
                pp_colour[i][j] =prev_colour[i][j];
                prev_colour[i][j]=colour[i][j];
               write_pixel(i,j,color_codes[clr]);
               colour[i][j]=color_codes[clr];
            }
        }
}
//score updation is being done here
void update_score(){
    char str[5];
   //a functional way to convert num to string
   sprintf(str, "%d", score);
    int a=0;
    int x=28;
    while (a<5) {
    write_char(x, 57, str[a]);
    x++;
    a++;
   }

    sprintf(str, "%d", level);
     a=0;
     x=58;
    while (a<5) {
    write_char(x, 57, str[a]);
    x++;
    a++;
   }
}
//shift the block
void generate_again(int x,int y,int size,int speed,int num){
    block_list[num].exists=0;
    for(int i=x-size;i<x+size;i++){
        for(int j=y;j<2*size+y;j++){
            colour[i][j]=prev_colour[i][j];
            prev_colour[i][j]=pp_colour[i][j];
            pp_colour[i][j]=0;
           write_pixel(i,j,0);
        }
    }
    score++;
    level=1+(score/5);
    update_score();
    block_list[num].y=0;
     block_list[num].x=0;
     return;
}
void move(int x,int y,int size,int speed,int num){
    if(y+2*size>210){
        check_collision(x,y+2*size-3);
        generate_again(x,y,size,speed,num);
       
        return;
    }
        for(int j=0;j<speed;j++){
        for(int i=x-size;i<x+size;i++){
            check_collision(i,y);
            if(exit_game==1) return;
            write_pixel(i,y,prev_colour[i][y]);

            colour[i][y]=prev_colour[i][y];
            prev_colour[i][y]=pp_colour[i][y];
            pp_colour[i][y]=0;
        }
        for(int i=x-size;i<x+size;i++){
            check_collision(i,y+2*size);
            if(exit_game==1) return;
            pp_colour[i][y+2*size]=prev_colour[i][y+2*size];
            prev_colour[i][y+2*size]=colour[i][y+2*size];
            write_pixel(i,y+2*size,colour[i][y+2*size-1]);
            colour[i][y+2*size]=colour[i][y+2*size-1];
        }
        y++;
        }
       
}
void print_block(block B,int i)
{
    
    int x = B.x, y = B.y, size = B.size, exists = B.exists;

    if (exists == 1)
    {
        int speed=level;
        block_list[i].y+=speed;
        move(x,y,size,speed,i);
    }
    else{
        if(rand()%3==0||rand()%3==1) return;
        block_list[i].exists=1;
        block_list[i].x=60+(i%3)*100;
        block_list[i].size=(rand()%10)+8; //8-17
        block_list[i].y=3;
        generate(block_list[i].x,block_list[i].size,i);
    }
}

void generate_mycar(){
     car_clr=0x00ab41;
    for(int i=160-10;i<160+10;i++){
        for(int j=200;j<215;j++){
            prev_colour[i][j]=0;
            colour[i][j]=car_clr;
            write_pixel(i,j,car_clr);
        }
    }
    car_x=160;
    car_y=207;
}
void generate_console(){
    char* hw = "Score:";
    int x=20;
   while (*hw) {
     write_char(x, 57, *hw);
	 x++;
	 hw++;
   }

	 hw = "Level:";
     x=45;
   while (*hw) {
     write_char(x, 57, *hw);
	 x++;
	 hw++;
   }

   hw = "Right:0";
     x=20;
   while (*hw) {
     write_char(x, 58, *hw);
	 x++;
	 hw++;
   }
   hw = "left:1";
     x=45;
   while (*hw) {
     write_char(x, 58, *hw);
	 x++;
	 hw++;
   }

   update_score();
}
void reset_var(){
    generate_mycar();
    exit_game=0;
    level=1;
    score=0;
    car_clr=0x00ab41;
    for(int i=0;i<240;i++){
        for(int j=0;j<32;j++){
            colour[i][j]=0;
            prev_colour[i][j]=0;
            pp_colour[i][j]=0;
        }
    }
    clear_screen();
    val=0;
    for(int i=0;i<number_blocks;i++){
        block_list[i].x=0;
        block_list[i].y=0;
        block_list[i].exists=0;
        block_list[i].speed=0;
        block_list[i].size=0;
    }
}
int main(){
   start_again:
     for(int i=0;i<5000000;i++){};
    reset_var();
    clear_screen();
    set_border();
    clear_char();
    generate_console();
    generate_mycar(50);
    time_t t;
    srand((unsigned)time(&t));
    int cnt=0;
    val=0;
    while(!exit_game){
        val = *PUSHBUTTONS;
        if(exit_game) goto start_again;
        if(cnt==0){
        int a=rand()%number_blocks;
        val = *PUSHBUTTONS;
        for(int i=0;i<25 && !exit_game;i++){
            if(exit_game) goto start_again;
            check_collide();
            check_button(val);
            print_block(block_list[a],a);
            for(int m=0;m<number_blocks && !exit_game;m++){
                if(exit_game) goto start_again;
                check_collide();
                check_button(val);
                if(block_list[m].exists){
                    print_block(block_list[m],m);
                }
            }
            for(int k=0;k<500000;k++){}
        }
        int b=rand()%number_blocks;
        val = *PUSHBUTTONS;
        for(int i=0;i<25 && !exit_game;i++){
            if(exit_game) goto start_again;
                check_collide();
                check_button(val);
            print_block(block_list[b],b);
            for(int m=0;m<number_blocks && !exit_game;m++){
                if(exit_game) goto start_again;
                check_collide();
                check_button(val);
                if(block_list[m].exists){
                    print_block(block_list[m],m);
                }
            }
            for(int k=0;k<500000;k++){}
        }
        int c=rand()%number_blocks;
        val = *PUSHBUTTONS;
        for(int i=0;i<25 && !exit_game;i++){
            if(exit_game) goto start_again;
                check_collide();
                check_button(val);
            print_block(block_list[c],c);
            for(int m=0;m<number_blocks && !exit_game;m++){
                if(exit_game) goto start_again;
                check_collide();
                check_button(val);
                if(block_list[m].exists){
                    
                    print_block(block_list[m],m);
                }
            }
            for(int k=0;k<500000;k++){}
        }
        int d=rand()%number_blocks;
        val = *PUSHBUTTONS;
        for(int i=0;i<25 && !exit_game;i++){
            if(exit_game) goto start_again;
                check_collide();
                check_button(val);
            print_block(block_list[d],d);
            for(int m=0;m<number_blocks && !exit_game;m++){
                if(exit_game) goto start_again;
                check_collide();
                check_button(val);
                if(block_list[m].exists){
                    print_block(block_list[m],m);
                }
            }
            for(int k=0;k<500000;k++){}
        }
        int e=rand()%number_blocks;
        val = *PUSHBUTTONS;
        for(int i=0;i<25 && !exit_game;i++){
            if(exit_game) goto start_again;
                check_collide();
                check_button(val);
            print_block(block_list[e],e);
            for(int m=0;m<number_blocks && !exit_game;m++){
                if(exit_game) goto start_again;
                check_collide();
                check_button(val);
                if(block_list[m].exists){
                    print_block(block_list[m],m);
                }
            }
            for(int k=0;k<500000;k++){}
        }
        int f=rand()%number_blocks;
        val = *PUSHBUTTONS;
        for(int i=0;i<25 && !exit_game;i++){
            if(exit_game) goto start_again;
                check_collide();
                check_button(val);
            print_block(block_list[f],f);
            for(int m=0;m<number_blocks && !exit_game;m++){
                if(exit_game) goto start_again;
                check_collide();
                check_button(val);
                if(block_list[m].exists){
                    print_block(block_list[m],m);
                }
            }
            for(int k=0;k<500000;k++){}
        }
        int g=rand()%number_blocks;
        val = *PUSHBUTTONS;
        for(int i=0;i<25 && !exit_game;i++){
            if(exit_game) goto start_again;
                check_collide();
                check_button(val);
            print_block(block_list[g],g);
            for(int m=0;m<number_blocks && !exit_game;m++){
                if(exit_game) goto start_again;
                check_collide();
                check_button(val);
                if(block_list[m].exists){
                    print_block(block_list[m],m);
                }
            }
            for(int k=0;k<500000;k++){}
        }
        int h=rand()%number_blocks;
        val = *PUSHBUTTONS;
        for(int i=0;i<25 && !exit_game;i++){
            if(exit_game) goto start_again;
                check_collide();
                check_button(val);
            print_block(block_list[h],h);
            for(int m=0;m<number_blocks && !exit_game;m++){
                if(exit_game) goto start_again;
                check_collide();
                check_button(val);
                if(block_list[m].exists){
                    print_block(block_list[m],m);
                }
            }
            for(int k=0;k<500000;k++){}
        }
        int z=rand()%number_blocks;
        val=*PUSHBUTTONS;
        for(int i=0;i<25 && !exit_game;i++){
            if(exit_game) goto start_again;
                check_collide();
                check_button(val);
            print_block(block_list[z],z);
            for(int m=0;m<number_blocks && !exit_game;m++){
                if(exit_game) goto start_again;
                check_collide();
                check_button(val);
                if(block_list[m].exists){
                    print_block(block_list[m],m);
                }
            }
            for(int k=0;k<500000;k++){}
        }
        }
        for(int i=0;i<number_blocks && !exit_game;i++){
            if(exit_game) goto start_again;
                check_collide();
                val = *PUSHBUTTONS;
                check_button(val);
            print_block(block_list[i],i);
        }
        for(int k=0;k<500000;k++){}
    }
   
    exit_game=0;
    goto start_again;
}