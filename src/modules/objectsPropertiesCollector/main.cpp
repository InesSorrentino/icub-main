/* 
 * Copyright (C) 2010 RobotCub Consortium, European Commission FP6 Project IST-004370
 * Author: Ugo Pattacini
 * email:  ugo.pattacini@iit.it
 * website: www.robotcub.org
 * Permission is granted to copy, distribute, and/or modify this program
 * under the terms of the GNU General Public License, version 2 or any
 * later version published by the Free Software Foundation.
 *
 * A copy of the license can be found at
 * http://www.robotcub.org/icub/license/gpl.txt
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details
*/

/**
@ingroup icub_module

\defgroup objectsPropertiesCollector objectsPropertiesCollector
 
Provides a on-line database to collect properties of objects 
that are of interest for your specific application.
 
Copyright (C) 2010 RobotCub Consortium
 
Author: Ugo Pattacini 
 
Date: first release 06/10/2010

CopyPolicy: Released under the terms of the GNU GPL v2.0. 
 
\section intro_sec Description 
 
Provides a on-line database to collect properties such as 
positions, colors, shapes, grasping points and so on about 
objects that are of interest for your specific application, i.e.
normally real objects that the robot can play with but ideally 
any kind of objects you can think of. 
The user can set, get, add, remove items and even make queries 
to the database. 

\section proto_sec Protocol
 
Notation: [.] is a Vocab, {.} is a string, <.> is a Value (i.e. 
string, double, int). 
 
Reserved properties tags: 
 
- \e id is used to specify the unique integer identifier 
assigned to ach stored item. 
 
- \e lifeTimer specifies the forgetting factor given in seconds, 
meaning that after \e lifeTimer seconds since its creation, the 
item is removed automatically from the database. 
 
The commands sent as bottles to the module port 
/<moduleName>/rpc are the following: 
 
<b>add</b> \n
Format: [add] (({prop0} <val0>) ({prop1} <val1>) ...) \n
Reply: [nack]; [ack] (id <num>) \n 
Action: a new item is added to the database with the given 
properties. A unique identifier is returned that is used to 
access the item. 

<b>remove</b> \n
Format: [del] ((id <num>)) \n 
Reply: [nack]; [ack] \n 
Action: remove from the database an item specified with the 
given identifier. \n
The special command "[del] (all)" clears the current content of 
the database. 
 
<b>get</b> \n
Format: [get] ((id <num>)) \n
Reply: [nack]; [ack] (({prop0} <val0>) ({prop1} <val1>) ...) \n
Action: return all the properties assigned to the stored item.
 
<b>set</b> \n
Format: [set] ((id <num>) ({prop2} <val2>) ...) \n 
Reply: [nack]; [ack] \n 
Action: add/modify properties of the stored item.
 
<b>dump</b> \n 
Format: [dump] \n 
Reply: [ack] \n 
Action: ask the database handler to dump on the screen all the 
stored items along with their properties. 
 
<b>ask</b> \n
Format: [ask] (({prop0} < <val0>) || ({prop1} >= <val1>) ...) \n
Reply: [nack]; [ack] (id (<num0> <num1> ...)) \n 
Action: query the database to find all the items whose 
properties match the conditions given in the command. You can 
compose multiple conditions using the boolean operators such as 
''||' for \e or and '&&' for \e and and each condition has to be 
expressed giving the property name, the value to compare with 
and the corresponding relational operator (e.g. >, <=, ==, 
...).\n 
The special command "[ask] (all)" returns the whole set of ids 
present within the database. \n 
Currently nested conditions of the form 
(cond1)&&((cond2)||(cond3)) are not handled. 
 
\section lib_sec Libraries 
- YARP libraries. 

\section parameters_sec Parameters
--name \e moduleName 
- The parameter \e moduleName identifies the module's name; all 
  the open ports will be tagged with the prefix /<moduleName>/.
  If not specified \e objectsPropertiesCollector is assumed.
 
--db \e dbFileName 
- The parameter \e dbFileName specifies the name of the database 
  to load at startup (if already existing) and save at shutdown.
 
--empty
- If this options is given then an empty database is started.
 
\section portsa_sec Ports Accessed
None.

\section portsc_sec Ports Created
 
- \e /<moduleName>/rpc the remote procedure call port used to 
  send requests to the database and receive replies.

\section in_files_sec Input Data Files
None.

\section out_data_sec Output Data Files
None. 
 
\section conf_file_sec Configuration Files
None. 
 
\section tested_os_sec Tested OS
Linux and Windows.

\section reqexample_sec Examples 
 
Several examples of the requests you may forward to the 
database: 
 
\code 
command: [add] ((name ball) (color red) (x 1)) 
reply: [ack] (id 0) 
 
command: [add] ((name octopus) (color blue) (x 2)) 
reply: [ack] (id 1) 
 
command: [set] ((id 1) (x 3)) 
reply: [ack] 
 
command: [get] ((id 1)) 
reply: [ack] ((id 1) (name octopus) (color blue) (x 3))
 
command: [ask] ((x < 10) && (color == blue)) 
reply: [ack] (id (1))
\endcode 
 
\author Ugo Pattacini
*/ 

#include <yarp/os/Property.h>
#include <yarp/os/Time.h>
#include <yarp/os/Network.h>
#include <yarp/os/PortReader.h>
#include <yarp/os/Semaphore.h>
#include <yarp/os/RFModule.h>

#include <stdio.h>
#include <string>
#include <map>
#include <deque>

using namespace yarp::os;
using namespace std;

#define CMD_ADD         VOCAB3('a','d','d')
#define CMD_DEL         VOCAB3('d','e','l')
#define CMD_GET         VOCAB3('g','e','t')
#define CMD_SET         VOCAB3('s','e','t')
#define CMD_DUMP        VOCAB4('d','u','m','p')
#define CMD_ASK         VOCAB3('a','s','k')

#define REP_ACK         VOCAB3('a','c','k')
#define REP_NACK        VOCAB4('n','a','c','k')
#define REP_UNKNOWN     VOCAB4('u','n','k','n')

#define OPT_ALL         VOCAB3('a','l','l')


namespace relationalOperators
{

/************************************************************************/
bool greater(Value &a, Value& b)
{
    if (a.isDouble() && b.isDouble())
        return (a.asDouble()>b.asDouble());
    else if (a.isInt() && b.isInt())
        return (a.asInt()>b.asInt());
    else
        return false;
}


/************************************************************************/
bool greaterEqual(Value &a, Value& b)
{
    if (a.isDouble() && b.isDouble())
        return (a.asDouble()>=b.asDouble());
    else if (a.isInt() && b.isInt())
        return (a.asInt()>=b.asInt());
    else
        return false;
}


/************************************************************************/
bool lower(Value &a, Value& b)
{
    if (a.isDouble() && b.isDouble())
        return (a.asDouble()<b.asDouble());
    else if (a.isInt() && b.isInt())
        return (a.asInt()<b.asInt());
    else
        return false;
}


/************************************************************************/
bool lowerEqual(Value &a, Value& b)
{
    if (a.isDouble() && b.isDouble())
        return (a.asDouble()<=b.asDouble());
    else if (a.isInt() && b.isInt())
        return (a.asInt()<=b.asInt());
    else
        return false;
}


/************************************************************************/
bool equal(Value &a, Value& b)
{
    if (a.isDouble() && b.isDouble())
        return (a.asDouble()==b.asDouble());
    else if (a.isInt() && b.isInt())
        return (a.asInt()==b.asInt());
    else if (a.isString() && b.isString())
    {
        string aStr=a.asString().c_str();
        string bStr=b.asString().c_str();

        return (aStr==bStr);
    }
    else
        return false;
}

}


/************************************************************************/
class DataBase
{
protected:
    map<int,Property*> itemsMap;
    Semaphore mutex;
    int idCnt;
    bool initialized;

    string dbFileName;

    /************************************************************************/
    struct Condition
    {
        string prop;
        bool (*compare)(Value&,Value&);
        Value val;
    };

    /************************************************************************/
    void clearMap()
    {
        for (map<int,Property*>::iterator it=itemsMap.begin(); it!=itemsMap.end(); it++)
            delete it->second;

        itemsMap.clear();
    }

    /************************************************************************/
    void eraseItem(map<int,Property*>::iterator &it)
    {
        delete it->second;
        itemsMap.erase(it);
    }

    /************************************************************************/
    void write(FILE *stream)
    {
        int i=0;
        for (map<int,Property*>::iterator it=itemsMap.begin(); it!=itemsMap.end(); it++)
            fprintf(stream,"item_%d (id %d) (%s)\n",i++,it->first,it->second->toString().c_str());
    }

public:
    /************************************************************************/
    DataBase()
    {
        initialized=false;
        idCnt=0;
    }

    /************************************************************************/
    ~DataBase()
    {
        save();
        clearMap();
    }

    /************************************************************************/
    void init(ResourceFinder &rf)
    {
        if (initialized)
        {
            fprintf(stdout,"database already initialized ...\n");
            return;
        }

        dbFileName=rf.findPath().c_str();
        dbFileName+=rf.find("db").asString().c_str();        

        if (!rf.check("empty"))
            load();

        dump();

        initialized=true;

        fprintf(stdout,"database ready ...\n");
    }

    /************************************************************************/
    void load()
    {
        return; // debug

        mutex.wait();

        clearMap();
        idCnt=0;

        Property finProperty;
        finProperty.fromConfigFile(dbFileName.c_str());
        Bottle finBottle(finProperty.toString().c_str());

        char tag[255];
        for (int i=0; i<finBottle.size(); i++)
        {
            sprintf(tag,"item_%d",i);
            Bottle &b=finBottle.findGroup(tag);

            if (b.isNull())
                continue;

            if (b.size()<2)
            {
                fprintf(stdout,"error while loading %s!\n",tag);
                continue;
            }

            Property idProp(b.get(0).asString().c_str());
            if (!idProp.check("id"))
            {
                fprintf(stdout,"%s does not have any id!\n",tag);
                continue;
            }

            int id=idProp.find("id").asInt();
            itemsMap[id]=new Property(b.get(1).asList()->toString().c_str());

            if (idCnt<id)
                idCnt=id+1;
        }

        fprintf(stdout,"database loaded from %s\n",dbFileName.c_str());

        mutex.post();
    }

    /************************************************************************/
    void save()
    {
        return; // debug

        mutex.wait();

        FILE *fout=fopen(dbFileName.c_str(),"w");
        write(fout);
        fclose(fout);

        fprintf(stdout,"database stored in %s\n",dbFileName.c_str());

        mutex.post();
    }

    /************************************************************************/
    void dump()
    {
        mutex.wait();

        fprintf(stdout,"dumping database content ... \n");        

        if (itemsMap.size()==0)
            fprintf(stdout,"empty\n");
        else
            write(stdout);

        mutex.post();
    }

    /************************************************************************/
    void add(Bottle *content)
    {
        mutex.wait();

        Property *item=new Property(content->toString().c_str());
        itemsMap[idCnt]=item;

        fprintf(stdout,"added item %s\n",item->toString().c_str());

        mutex.post();
    }

    /************************************************************************/
    bool remove(Bottle *content)
    {
        mutex.wait();

        if (content->size()==1)
            if (content->get(0).isVocab() || content->get(0).isString())
                if (content->get(0).asVocab()==OPT_ALL)
                {
                    clearMap();
                    fprintf(stdout,"database cleared\n");
                    mutex.post();
                    return true;
                }

        Property request(content->toString().c_str());

        if (!request.check("id"))
        {
            fprintf(stdout,"id field not present within the request!\n");
            mutex.post();
            return false;
        }
        
        int id=request.find("id").asInt();
        fprintf(stdout,"removing item %d ... ",id);

        map<int,Property*>::iterator it=itemsMap.find(id);
        if (it!=itemsMap.end())
        {
            eraseItem(it);
            fprintf(stdout,"successfully\n");
            mutex.post();
            return true;
        }
        
        fprintf(stdout,"not present!\n");

        mutex.post();

        return false;
    }

    /************************************************************************/
    bool get(Bottle *content, Bottle &item)
    {
        mutex.wait();

        Property request(content->toString().c_str());

        if (!request.check("id"))
        {
            fprintf(stdout,"id field not present within the request!\n");
            mutex.post();
            return false;
        }

        int id=request.find("id").asInt();
        fprintf(stdout,"getting item %d ... ",id);

        map<int,Property*>::iterator it=itemsMap.find(id);
        if (it!=itemsMap.end())
        {
            item.clear();
            item.fromString(it->second->toString().c_str());
            fprintf(stdout,"%s\n",item.toString().c_str());
            mutex.post();
            return true;
        }

        fprintf(stdout,"not present!\n");

        mutex.post();

        return false;
    }

    /************************************************************************/
    bool set(Bottle *content)
    {
        mutex.wait();

        Property request(content->toString().c_str());

        if (!request.check("id"))
        {
            fprintf(stdout,"id field not present within the request!\n");
            mutex.post();
            return false;
        }

        int id=request.find("id").asInt();
        fprintf(stdout,"setting item %d ... ",id);

        map<int,Property*>::iterator it=itemsMap.find(id);
        if (it!=itemsMap.end())
        {
            request.unput("id");
            Bottle b(request.toString().c_str());

            fprintf(stdout,"%s\n",b.toString().c_str());

            for (int i=0; i<b.size(); i++)
            {
                Bottle *option=b.get(i).asList();

                if (option->size()<2)
                {
                    fprintf(stdout,"invalid property!\n");
                    continue;
                }

                string prop=option->get(0).asString().c_str();
                Value  val=option->get(1);

                it->second->unput(prop.c_str());
                it->second->put(prop.c_str(),val);
            }

            mutex.post();

            return true;
        }

        fprintf(stdout,"not present!\n");

        mutex.post();

        return false;
    }

    /************************************************************************/
    bool ask(Bottle *content, Bottle &items)
    {
        mutex.wait();

        if (content->size()==1)
            if (content->get(0).isVocab() || content->get(0).isString())
                if (content->get(0).asVocab()==OPT_ALL)
                {
                    items.clear();

                    for (map<int,Property*>::iterator it=itemsMap.begin(); it!=itemsMap.end(); it++)
                        items.addInt(it->first);

                    mutex.post();
                    return true;
                }

        deque<Condition> condList;
        deque<string>    opList;

        // we cannot accept a conditions string ending with
        // a boolean operator
        if (!(content->size()&0x01))
        {
            fprintf(stdout,"uncorrect conditions received!\n");
            mutex.post();
            return false;
        }

        // parse the received conditions and build the lists
        for (int i=0; i<content->size(); i+=2)
        {
            Bottle *b=content->get(i).asList();
            Condition condition;
            string operation;

            if (b->size()<3)
            {
                fprintf(stdout,"condition given with less than 3 elements!\n");
                mutex.post();
                return false;
            }
            
            condition.prop=b->get(0).asString().c_str();
            condition.val=b->get(2);

            operation=b->get(1).asString().c_str();
            if (operation==">")
                condition.compare=&relationalOperators::greater;
            else if (operation==">=")
                condition.compare=&relationalOperators::greaterEqual;
            else if (operation=="<")
                condition.compare=&relationalOperators::lower;
            else if (operation=="<=")
                condition.compare=&relationalOperators::lowerEqual;
            else if (operation=="==")
                condition.compare=&relationalOperators::equal;
            else
            {
                fprintf(stdout,"unknown relational operator '%s'!\n",operation.c_str());
                mutex.post();
                return false;
            }

            condList.push_back(condition);

            if ((i+1)<content->size())
            {
                operation=content->get(i+1).asString().c_str();
                if ((operation!="||") && (operation!="&&"))
                {
                    fprintf(stdout,"unknown boolean operator '%s'!\n",operation.c_str());
                    mutex.post();
                    return false;
                }
                else
                    opList.push_back(operation);
            }
        }

        items.clear();

        // apply the conditions to each item
        for (map<int,Property*>::iterator it=itemsMap.begin(); it!=itemsMap.end(); it++)
        {
            // there must be at least one condition to process
            string &prop=condList[0].prop;
            bool finalRes;

            if (it->second->check(prop.c_str()))
            {
                Value &val=it->second->find(prop.c_str());
                finalRes=(*condList[0].compare)(val,condList[0].val);
            }
            else
                finalRes=false;

            // if we're required to process more than one condition
            // we go ahead accumulating the temporary results
            for (unsigned int j=0; j<opList.size(); j++)
            {
                int k=j+1;

                string &prop=condList[k].prop;
                bool currentRes;

                if (it->second->check(prop.c_str()))
                {
                    Value &val=it->second->find(prop.c_str());
                    currentRes=(*condList[k].compare)(val,condList[k].val);
                }
                else
                    currentRes=false;

                if (opList[j]=="||")
                    finalRes=finalRes||currentRes;
                else if (opList[j]=="&&")
                    finalRes=finalRes&&currentRes;
            }

            // keep only the item that satisfies the whole list of conditions
            if (finalRes)
                items.addInt(it->first);
        }

        fprintf(stdout,"found items matching received conditions (%s)\n",items.toString().c_str());

        mutex.post();

        return true;
    }

    /************************************************************************/
    void periodicHandler(const double dt)   // manage the items life-timers
    {
        mutex.wait();

        for (map<int,Property*>::iterator it=itemsMap.begin(); it!=itemsMap.end(); it++)
        {
            if (it->second->check("lifeTimer"))
            {
                double lifeTimer=it->second->find("lifeTimer").asDouble()-dt;

                if (lifeTimer<0.0)
                {
                    fprintf(stdout,"item with id=%d expired\n",it->first);
                    eraseItem(it);

                    // bug ?? a run-time error occurs if we don't break here;
                    // never mind: at the next call we keep on checking
                    break;
                }
                else
                {
                    it->second->unput("lifeTimer");
                    it->second->put("lifeTimer",lifeTimer);
                }
            }
        }

        mutex.post();
    }

    /************************************************************************/
    void respond(const Bottle &command, Bottle &reply)
    {
        if (command.size()<1)
        {
            reply.addVocab(REP_NACK);
            return;
        }

        reply.clear();

        int cmd=command.get(0).asVocab();

        switch(cmd)
        {
            case CMD_ADD:
            {
                if (command.size()<2)
                {
                    reply.addVocab(REP_NACK);
                    break;
                }

                Bottle *content=command.get(1).asList();

                add(content);
                reply.addVocab(REP_ACK);
                Bottle &b=reply.addList();
                b.addString("id");
                b.addInt(idCnt);
                idCnt++;

                break;
            }

            case CMD_DEL:
            {
                if (command.size()<2)
                {
                    reply.addVocab(REP_NACK);
                    break;
                }

                Bottle *content=command.get(1).asList();

                if (remove(content))
                    reply.addVocab(REP_ACK);
                else
                    reply.addVocab(REP_NACK);

                break;
            }

            case CMD_GET:
            {
                if (command.size()<2)
                {
                    reply.addVocab(REP_NACK);
                    break;
                }

                Bottle *content=command.get(1).asList();
                Bottle item;

                if (get(content,item))
                {
                    reply.addVocab(REP_ACK);
                    reply.addList()=item;
                }
                else
                    reply.addVocab(REP_NACK);

                break;
            }

            case CMD_SET:
            {
                if (command.size()<2)
                {
                    reply.addVocab(REP_NACK);
                    break;
                }
    
                Bottle *content=command.get(1).asList();
    
                if (set(content))
                    reply.addVocab(REP_ACK);
                else
                    reply.addVocab(REP_NACK);
    
                break;
            }
    
            case CMD_DUMP:
            {
                dump();
                reply.addVocab(REP_ACK);
                break;
            }

            case CMD_ASK:
            {
                if (command.size()<2)
                {
                    reply.addVocab(REP_NACK);
                    break;
                }

                Bottle *content=command.get(1).asList();
                Bottle items;

                if (ask(content,items))
                {
                    reply.addVocab(REP_ACK);
                    Bottle &b=reply.addList();
                    b.addString("id");
                    b.addList()=items;
                }
                else
                    reply.addVocab(REP_NACK);

                break;
            }

            default:
            {
                fprintf(stdout,"received unknown command!\n");
                reply.addVocab(REP_UNKNOWN);
            }
        }
    }
};


/************************************************************************/
class RpcProcessor : public PortReader
{
protected:
    DataBase *pDataBase;

    /************************************************************************/
    virtual bool read(ConnectionReader &connection)
    {
        Bottle command, reply;

        if (!command.read(connection))
            return false;

        pDataBase->respond(command,reply);

        if (ConnectionWriter *writer=connection.getWriter())
            reply.write(*writer);

        return true;
    }

public:
    /************************************************************************/
    void setDataBase(DataBase &dataBase)
    {
        pDataBase=&dataBase;
    }
};


/************************************************************************/
class objectsPropertiesCollectorModule: public RFModule
{
private:
    DataBase     dataBase;
    RpcProcessor rpcProcessor;
    Port         rpcPort;

    int cnt;

public:
    /************************************************************************/
    virtual bool configure(ResourceFinder &rf)
    {
        Time::turboBoost();

        dataBase.init(rf);

        string name=rf.check("name",Value("objectsPropertiesCollector")).asString().c_str();
        rpcProcessor.setDataBase(dataBase);
        rpcPort.setReader(rpcProcessor);
        rpcPort.open(("/"+name+"/rpc").c_str());

        cnt=0;

        return true;
    }

    /************************************************************************/
    virtual bool close()
    {
        rpcPort.interrupt();
        rpcPort.close();

        return true;
    }

    /************************************************************************/
    virtual bool updateModule()
    {        
        dataBase.periodicHandler(getPeriod());

        // back-up straightaway the database each 15 minutes
        if ((++cnt)*getPeriod()>(15.0*60.0))
        {
            dataBase.save();
            cnt=0;
        }

        return true;
    }

    /************************************************************************/
    virtual double getPeriod()
    {
        return 1.0;
    }
};


/************************************************************************/
int main(int argc, char *argv[])
{
    ResourceFinder rf;
    rf.setVerbose(true);
    rf.setDefaultContext("objectsPropertiesCollector/conf");
    rf.setDefault("db","dataBase.ini");
    rf.configure("ICUB_ROOT",argc,argv);

    if (rf.check("help"))
    {
        fprintf(stdout,"Options\n\n");
        fprintf(stdout,"\t--name        <name>: collector name (default: objectsPropertiesCollector)\n");
        fprintf(stdout,"\t--db      <fileName>: database file name to load at startup/save at shutdown (default: dataBase.ini)\n");
        fprintf(stdout,"\t--context  <context>: context to search for database file (default: objectsPropertiesCollector/conf)\n");
        fprintf(stdout,"\t--empty             : start an empty database\n");
        fprintf(stdout,"\n");

        return 0;
    }

    Network yarp;

    if (!yarp.checkNetwork())
        return -1;

    objectsPropertiesCollectorModule collector;

    return collector.runModule(rf);
}



