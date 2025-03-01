//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#include "Scheduler.h"
#include <algorithm>
#include <vector>

Define_Module(Scheduler);



Scheduler::Scheduler()
{
    selfMsg = nullptr;
}

Scheduler::~Scheduler()
{
    cancelAndDelete(selfMsg);
}


void Scheduler::initialize()
{
    NrUsers = par("gateSize").intValue();
    NrOfChannels = 14;
    selfMsg = new cMessage("selfMsg");

    for(int i=0; i<NrUsers;i++){
           q[i]=0;
           NrBlocks[i]=0;
           //weights[i] = i+1;
           if(i<NrUsers/3) {
               weights[i] = par("W_LP").intValue(); //LP
           }
           else if(i<2*(NrUsers/3)){
               weights[i] = par("W_MP").intValue(); //MP
           }
           else if(i<NrUsers){
               weights[i] = par("W_HP").intValue(); //HP
           }
           radioQuality[i] = uniform(1, 10);
           lastServedTime[i] = simTime();

           EV << "Initialize user: " << i <<" weight: " << weights[i]
                   << " Radio Quality: " << radioQuality[i] << endl;
    }
    scheduleAt(simTime(), selfMsg);


}

void Scheduler::handleMessage(cMessage *msg)
{

    for(int i=0;i < NrUsers;i++){
        if (msg->arrivedOn("rxInfo", i)){
            q[i]= msg->par("ql_info");
            EV << "Update: q["<<i<<"]= " << q[i] <<endl;
            delete(msg);
        }
    }

    if (msg == selfMsg){

        memset(NrBlocks, 0, sizeof(NrBlocks));
        int totalBlocks = NrOfChannels;
        int remainingBlocks = totalBlocks;
        double priorities[NrUsers];
        double totalPriority = 0.0;

        for(int i=0; i<NrUsers;i++){
            radioQuality[i] = uniform(1, 10);
        }

        //round robin in numerical order
        /*while (remainingBlocks > 0) {
            bool anyAllocationMade = false;

            for (int i = 0; i < NrUsers; i++) {
                // Only allocate blocks to users with a non-zero queue length
                if (q[i] > 0 && remainingBlocks > 0) {
                    NrBlocks[i]++;
                    remainingBlocks--;
                    anyAllocationMade = true;

                    lastServedTime[i] = simTime();
                }
            }

            // Exit the loop if no allocation was made (i.e., all users' queues are empty)
            if (!anyAllocationMade) {
                break;
            }
        }*/

        std::vector<std::pair<int, double>> userPriorities;

        for (int i = 0; i < NrUsers; i++) {

                   //update weighted value based on FLC modifications
                   if(i>=2*(NrUsers/3)){
                           weights[i] = par("W_HP").intValue();
                   }

                   EV << "User: " << i <<" New weight: " << weights[i] << endl;

                   //OS
                   //priorities[i] = radioQuality[i];

                   //RR - time priority
                   //priorities[i] = simTime().dbl() - lastServedTime[i].dbl();

                   //WRR
                   priorities[i] = weights[i] * (simTime().dbl() - lastServedTime[i].dbl());

                   //PF
                   //priorities[i] = radioQuality[i] *  weights[i] * (simTime().dbl() - lastServedTime[i].dbl());

                   //LQ
                   //priorities[i] = q[i] * weights[i];

                   totalPriority += priorities[i];
                   userPriorities.push_back({i, priorities[i]});
               }

        std::sort(userPriorities.begin(), userPriorities.end(),
                          [](const std::pair<int, double>& a, const std::pair<int, double>& b) {
                              return a.second > b.second;  // Sort in descending order
                          });



        for (const auto& user : userPriorities) {
                int userIndex = user.first;
                double userPriority = user.second;

                if (remainingBlocks > 0 && q[userIndex] > 0) {

                    //the best channel from the remaining empty channels
                    int allocatedBlocks = std::ceil((userPriority / totalPriority) * remainingBlocks);
                    allocatedBlocks = std::min(allocatedBlocks, q[userIndex]); // Do not allocate more than available in queue
                    allocatedBlocks = std::min(allocatedBlocks, remainingBlocks); // Do not exceed remaining blocks

                    NrBlocks[userIndex] = allocatedBlocks;
                    remainingBlocks -= allocatedBlocks;


                    if (NrBlocks[userIndex] > 0) {
                        lastServedTime[userIndex] = simTime();
                    }
                }
            }


        // Allocate remaining blocks based on priority
        for (const auto& user : userPriorities) {
                    int userIndex = user.first;
                    if (remainingBlocks > 0 && q[userIndex] > NrBlocks[userIndex]) {
                        NrBlocks[userIndex]++;
                        remainingBlocks--;

                        lastServedTime[userIndex] = simTime();
                    }
                }





        for(int i=0;i < NrUsers;i++){
            if(NrBlocks[i] > 0){
                cMessage *cmd = new cMessage("cmd");
                cmd->addPar("nrBlocks");
                cmd->par("nrBlocks").setLongValue(NrBlocks[i]);
                send(cmd,"txScheduling",i);
                EV << "Allocated " << NrBlocks[i] << " blocks to user " << i
                                   << " (Priority: " << priorities[i] << ")" << "Available in queue: " << q[i] << endl;
            }
        }

        //double transfer_rate = NrOfChannels / par("schedulingPeriod").doubleValue();
        //double data_generated = (NrUsers * (totalBlocks - remainingBlocks) * 3) / 10ms;

        EV << "Remaining Blocks: " << remainingBlocks << endl;

        scheduleAt(simTime()+par("schedulingPeriod").doubleValue(), selfMsg);
    }

//    if (msg->arrivedOn("rxMeanDelay")) {
//            // Retrieve the mean delay from Sink
//            double meanHPDelay = msg->par("meanHPDelay").doubleValue();
//            EV << "Received mean delay from Sink: " << meanHPDelay << "s" << endl;
//
//            // Send mean delay to FLC
//            cMessage *flcMsg = new cMessage("meanHPDelayToFLC");
//            flcMsg->addPar("meanHPDelay") = meanHPDelay;
//            send(flcMsg, "schedOut");
//        }

}
