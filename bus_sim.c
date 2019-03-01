//External definitions for bus model

#include "simlib.h"
#include <unistd.h>

#define EVENT_ARRIVAL_1         1   //Person arrives in air station 1
#define EVENT_ARRIVAL_2         2   //Person arrives in air station 2   
#define EVENT_ARRIVAL_3         3   //Person arrives in car rental 3
#define EVENT_BUS_ARRIVE        4   //Bus arrives
#define EVENT_BUS_DEPART        5   //Bus departs
#define EVENT_END               6   //Simulation ends

#define LIST_QUEUE_1            1   //Queue on air station 1
#define LIST_QUEUE_2            2   //Queue on air station 2
#define LIST_QUEUE_3            3   //Queue on car rental
#define LIST_QUEUE_BUS_1        4   //Queue on bus going to air station 1
#define LIST_QUEUE_BUS_2        5   //Queue on bus going to air station 2
#define LIST_QUEUE_BUS_3        6   //Queue on bus going to car rental 3
#define LIST_BUS                7   //Bus idle status

#define STREAM_INTERARRIVAL_1   1   //Random stream of air station 1 arrival
#define STREAM_INTERARRIVAL_2   2   //Random stream of air station 2 arrival
#define STREAM_INTERARRIVAL_3   3   //Random stream of car rental 3
#define STREAM_UNLOAD           4   //Random stream of unloading time
#define STREAM_LOAD             5   //Random stream of loading time
#define STREAM_DESTINATION      6   //Random stream of destination from car rental 3

#define SAMPST_NUM_1            1   //Number in station 1 queue
#define SAMPST_NUM_2            2   //Number in station 2 queue
#define SAMPST_NUM_3            3   //Number in car rental 3 queue
#define SAMPST_DELAYS_1         4   //Delay in station 1 queue
#define SAMPST_DELAYS_2         5   //Delay in station 2 queue
#define SAMPST_DELAYS_3         6   //Delay in car rental queue
#define SAMPST_NUM_BUS          7   //Number on bus
#define SAMPST_LOC_DELAYS_1     8   //Time stopping on station 1
#define SAMPST_LOC_DELAYS_2     9   //Time stopping on station 2
#define SAMPST_LOC_DELAYS_3     10  //Time stopping on car rental 3
#define SAMPST_LOOP_DELAY       11  //Time from car rental 3 to car rental 3 again
#define SAMPST_DELAY            12  //Total service time


//Application specific definitions
#define num_locations 3
#define max_bus_seats 20

//non-simlib global variables
int sim_length;
int waiting_time;
int min_load_time, max_load_time;;
int min_unload_time, max_unload_time;
int bus_position;
int seat_taken;
int last_loop_time;
double travel_time[3];
double interarrival_time[3];
double dest_probability[3];
FILE *infile, *outfile;

//user-defined functions
void init_model();
void arrive(int station);
void bus_arrive();               
void bus_depart();               
int load(int station);
int unload(int station);
void report();

int main()
{
    //Parameters and log
    infile = fopen ("bus_sim.in", "r");
    outfile = fopen ("bus_sim.out", "w");

    // Mean interarrival rate per hour at location 1, 2, 3 
    for (int i = 0; i < num_locations; ++i) {
        fscanf (infile, "%lg", &interarrival_time[i]);
        // mean interarrival in seconds
        interarrival_time[i] = 3600.0/interarrival_time[i]; 
    }

    // Destination probability distribution to 1, 2, 3
    for (int i = 0; i < num_locations; ++i)
        fscanf (infile, "%lg", &dest_probability[i]);

    // Travel time to 1, 2, 3 in seconds
    for (int i = 0; i < num_locations; ++i)
        fscanf (infile, "%lg", &travel_time[i]);

    // Unload times in seconds
    fscanf (infile, "%d %d", &min_unload_time, &max_unload_time);

    // Load times in seconds
    fscanf (infile, "%d %d", &min_load_time, &max_load_time);

    // Idle waiting time in minutes
    fscanf (infile, "%d", &waiting_time);
    //convert to seconds
    waiting_time = waiting_time * 60;

    //Simulation length in hours
    fscanf (infile, "%d", &sim_length);
    //convert to seconds
    sim_length *= 3600;

    fprintf(outfile, "Mean interarrival time %11.3f %11.3f %11.3f seconds\n", interarrival_time[0], interarrival_time[1], interarrival_time[2]);
    fprintf(outfile, "Destination probability %11.3f %11.3f %11.3f \n", dest_probability[0], dest_probability[1], dest_probability[2]);
    fprintf(outfile, "Travel time %11.3f %11.3f %11.3f seconds\n", travel_time[0], travel_time[1], travel_time[2]);
    fprintf(outfile, "Unload time min %d, max %d seconds\n", min_unload_time, max_unload_time);
    fprintf(outfile, "Load time min %d, max %d seconds\n", min_load_time, max_load_time);
    fprintf(outfile, "Idle waiting time min %d seconds\n", waiting_time);
    fprintf(outfile, "Simulated time %d hours\n", sim_length/3600);

    init_simlib();

    maxatr = 4;

    init_model();

    do
    {
        timing();
        switch(next_event_type)
        {
            case EVENT_ARRIVAL_1:
                arrive(1);
                break;
            case EVENT_ARRIVAL_2:
                arrive(2);
                break;
            case EVENT_ARRIVAL_3:
                arrive(3);
                break;
            case EVENT_BUS_ARRIVE:
                bus_arrive();
                break;
            case EVENT_BUS_DEPART:
                bus_depart();
                break;
            case EVENT_END:
                report();
                break;
        }
    }
    while (next_event_type != EVENT_END);

    fclose(infile);
    fclose(outfile);

    return 0;
}

void init_model()
{
    //initialize simulation end event
    event_schedule(sim_length, EVENT_END);

    //initialize arrivals
    event_schedule (expon (interarrival_time[0], STREAM_INTERARRIVAL_1), EVENT_ARRIVAL_1);
    event_schedule (expon (interarrival_time[1], STREAM_INTERARRIVAL_2), EVENT_ARRIVAL_2);
    event_schedule (expon (interarrival_time[2], STREAM_INTERARRIVAL_3), EVENT_ARRIVAL_3);
    
    //initialize bus position and first event
    bus_position = 3;
    event_schedule(sim_time, EVENT_BUS_DEPART);

    seat_taken = 0;
    last_loop_time = sim_time;
}

void arrive(int station)
{
    //take variable outside since transfer arrays cannot be in inclusive blocks
    int dest;
    //record destination as attr2 and put to queue
    if (station == 1) 
    {
		event_schedule (sim_time + expon (interarrival_time[0], STREAM_INTERARRIVAL_1), EVENT_ARRIVAL_1);
		dest = 3;
	} 
    else if (station == 2) 
    {
		event_schedule (sim_time + expon (interarrival_time[1], STREAM_INTERARRIVAL_2), EVENT_ARRIVAL_2);
		dest = 3;
	} 
    else if (station == 3) 
    {
		event_schedule (sim_time + expon (interarrival_time[2], STREAM_INTERARRIVAL_3), EVENT_ARRIVAL_3);
		int prob_dest = lcgrand(STREAM_DESTINATION);
		if (prob_dest >= dest_probability[1]) 
			dest = 1;
		else 
			dest = 2;
    } 

    //record arrival time
    transfer[1] = sim_time;
    transfer[2] = dest;
    list_file(LAST, LIST_QUEUE_1 + (station - 1));

}

void bus_arrive()
{
    if (bus_position == 3)
    {
        bus_position = 1;
    }
    else
    {
        bus_position++;

        if (bus_position == 3) 
        {
            //count loop time
            sampst(sim_time - last_loop_time, SAMPST_LOOP_DELAY);
            last_loop_time = sim_time;
        }
    }
    
    //departure handled in each
    int park_time = unload(bus_position);
    park_time += load(bus_position);
    if (park_time <= waiting_time)
        sampst(waiting_time, SAMPST_LOC_DELAYS_1 + (bus_position - 1));
    else
        sampst(park_time, SAMPST_LOC_DELAYS_1 + (bus_position - 1));    

    sampst(seat_taken, SAMPST_NUM_BUS);
}

void bus_depart()
{
    int dest = 1;
    if (bus_position <=2)
    {
        dest = bus_position + 1;
    }
    event_schedule(sim_time+travel_time[dest-1], EVENT_BUS_ARRIVE);
}

int unload(int station)
{
    int unload_time = 0;
    while (list_size[LIST_QUEUE_BUS_1 + (station -1)] > 0)
    {
        unload_time += uniform(min_unload_time, max_unload_time, STREAM_UNLOAD);
        list_remove(FIRST, LIST_QUEUE_BUS_1 + (station -1));
        --seat_taken;
        sampst(sim_time - transfer[1] + unload_time, SAMPST_DELAY);
        list_remove(FIRST, LIST_BUS);
    }
    event_cancel(EVENT_BUS_DEPART);
    event_schedule(sim_time + waiting_time + unload_time, EVENT_BUS_DEPART);
    return unload_time;
}

int load(int station)
{
    int load_time = 0;
    //if there are arrival within wait time, wait.
    if (next_event_type == EVENT_ARRIVAL_1 + (station - 1))
        timing();
    while (list_size[LIST_QUEUE_1 + (station -1)] > 0 && seat_taken<20)
    {
        load_time += uniform(min_load_time, max_load_time, STREAM_LOAD);
        //attr2 is destination
        list_remove(FIRST, LIST_QUEUE_1 + (station -1));

        sampst((sim_time - transfer[1]) + load_time, SAMPST_DELAYS_1 + (station - 1));
        
        list_file(LAST, LIST_QUEUE_BUS_1 + (transfer[2] -1));
        ++seat_taken;

        event_cancel(EVENT_BUS_DEPART);
        event_schedule(sim_time + waiting_time + load_time, EVENT_BUS_DEPART);
        list_file(LAST, LIST_BUS);
    }
    return load_time;
}

void report()
{
    fprintf(outfile, "\nQueue length in air station 1:\n");
    out_filest(outfile, LIST_QUEUE_1,LIST_QUEUE_1);
    fprintf(outfile, "\nQueue length in air station 2:\n");
    out_filest(outfile, LIST_QUEUE_2,LIST_QUEUE_2);
    fprintf(outfile, "\nQueue length in car rental 3:\n");
    out_filest(outfile, LIST_QUEUE_3,LIST_QUEUE_3);
    fprintf(outfile, "\n\n");
    fprintf(outfile, "\nQueue delay in air station 1:\n");         
    out_sampst(outfile, SAMPST_DELAYS_1, SAMPST_DELAYS_1);
    fprintf(outfile, "\nQueue delay in air station 2:\n");
    out_sampst(outfile, SAMPST_DELAYS_2, SAMPST_DELAYS_2);
    fprintf(outfile, "\nQueue delay in car rental 3:\n");
    out_sampst(outfile, SAMPST_DELAYS_3, SAMPST_DELAYS_3);
    fprintf(outfile, "\n\n");
    fprintf(outfile, "\nQueue length inside bus:\n");
    out_filest(outfile, LIST_BUS, LIST_BUS);
    fprintf(outfile, "\n\n");
    fprintf(outfile, "\nWaiting time in air station 1:\n");
    out_sampst(outfile, SAMPST_LOC_DELAYS_1, SAMPST_LOC_DELAYS_1);
    fprintf(outfile, "\nWaiting time in air station 2:\n");
    out_sampst(outfile, SAMPST_LOC_DELAYS_2, SAMPST_LOC_DELAYS_2);
    fprintf(outfile, "\nWaiting time in air station 3:\n");
    out_sampst(outfile, SAMPST_LOC_DELAYS_3, SAMPST_LOC_DELAYS_3);
    fprintf(outfile, "\n\n");
    fprintf(outfile, "\nLoop Delay:\n");                                
    out_sampst(outfile, SAMPST_LOOP_DELAY, SAMPST_LOOP_DELAY);
    fprintf(outfile, "\n\n");
    fprintf(outfile, "\nService time:\n");
    out_sampst(outfile, SAMPST_DELAY,SAMPST_DELAY);

}