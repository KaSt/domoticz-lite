#include "stdafx.h"
#include "1WireByOWFS.h"

#include <fstream>
#include <algorithm>

#ifdef WIN32
#include "../../main/dirent_windows.h"
#else
#include <dirent.h>
#endif

#include "../../main/Helper.h"

#ifdef _DEBUG
#ifdef WIN32
#define OWFS_Base_Dir "E:\\w1\\1wire\\uncached"
#else // WIN32
#define OWFS_Base_Dir "/mnt/1wire/uncached"
#endif // WIN32
#else // _DEBUG
#define OWFS_Base_Dir "/mnt/1wire/uncached"
#endif //_DEBUG

#define HUB_MAIN_SUB_PATH     "/main"
#ifdef WIN32
#define HUB_AUX_SUB_PATH      "/_aux"
#else
#define HUB_AUX_SUB_PATH      "/aux"
#endif


bool C1WireByOWFS::IsAvailable()
{
    DIR *d=NULL;

    d=opendir(OWFS_Base_Dir);
    if (d != NULL)
    {
        struct dirent *de=NULL;
        // Loop while not NULL
        while ((de = readdir(d)))
        {
            std::string dirname = de->d_name;
            if (de->d_type==DT_DIR)
            {
                if ((dirname!=".")&&(dirname!=".."))
                {
                    closedir(d);
                    return true;
                }
            }
        }
        closedir(d);
    }
    return false;
}

bool C1WireByOWFS::FindDevice(const std::string &sID, /*out*/_t1WireDevice& device) const
{
    return FindDevice(OWFS_Base_Dir, sID, device);
}

bool C1WireByOWFS::FindDevice(const std::string &inDir, const std::string &sID, /*out*/_t1WireDevice& device) const
{
    bool found = false;
    DIR *d=opendir(inDir.c_str());
    if (d != NULL)
    {
        struct dirent *de=NULL;
        // Loop while not NULL or not found
        while((de=readdir(d)) && !found)
        {
            // Check dir
            if (!IsValidDir(de))
                continue;

            // Get the device from it's dirname
            GetDevice(inDir, de->d_name, device);

            // Check if it's the device we are looking for
            if (device.devid.compare(0,sID.length(),sID)==0)
            {
                found=true;
                continue;
            }

            // Else, try to scan hubs (recursively)
            if (device.family==microlan_coupler)
            {
                // Search in "main" and "aux" dir
                found=FindDevice(inDir + "/" + de->d_name + HUB_MAIN_SUB_PATH, sID, device);
                if(!found)
                    found=FindDevice(inDir + "/" + de->d_name + HUB_AUX_SUB_PATH, sID, device);
            }
        }
    }

    closedir(d);
    return found;
}

void C1WireByOWFS::GetDevices(/*out*/std::vector<_t1WireDevice>& devices) const
{
    return GetDevices(OWFS_Base_Dir, devices);
}

void C1WireByOWFS::GetDevices(const std::string &inDir, /*out*/std::vector<_t1WireDevice>& devices) const
{
    DIR *d=opendir(inDir.c_str());
    if (d != NULL)
    {
        struct dirent *de=NULL;
        // Loop while not NULL
        while((de=readdir(d)))
        {
            // Check dir
            if (!IsValidDir(de))
                continue;

            // Get the device from it's dirname
            _t1WireDevice device;
            GetDevice(inDir, de->d_name, device);

            // Add device to list
            devices.push_back(device);

            // If device is a hub, scan for all hub connected devices (recursively)
            if (device.family==microlan_coupler)
            {
                // Scan in "main" and "aux" dir
                GetDevices(inDir + "/" + de->d_name + HUB_MAIN_SUB_PATH, devices);
                GetDevices(inDir + "/" + de->d_name + HUB_AUX_SUB_PATH, devices);
            }
        }
    }

    closedir(d);
}

std::string C1WireByOWFS::readRawData(const std::string& filename) const
{
    std::ifstream file;
    file.open(filename.c_str());
    if (file.is_open())
    {
        std::string sLine;
        getline(file, sLine);
        file.close();
		if (is_number(sLine))
			return sLine;
    }
    return "";
}

void C1WireByOWFS::writeData(const _t1WireDevice& device,std::string propertyName,const std::string &value) const
{
    std::ofstream file;
    file.open(std::string(device.filename+"/"+propertyName).c_str());
    if (!file.is_open())
        return;

    file << value;
    file.close();
}

void C1WireByOWFS::SetLightState(const std::string& sId,int unit,bool value)
{
   _t1WireDevice device;
   if (!FindDevice(sId, device))
      return;

   switch(device.family)
   {
   case Addresable_Switch:
      {
         writeData(device,"PIO",value?"yes":"no");
         break;
      }
   case dual_addressable_switch_plus_1k_memory:
   case Temperature_IO:
   case dual_channel_addressable_switch:
      {
         if (unit<0 || unit>1)
            return;
         writeData(device,std::string("PIO.").append(1,'A'+unit),value?"yes":"no");
         break;
      }
   case _8_channel_addressable_switch:
      {
         if (unit<0 || unit>7)
            return;
         writeData(device,std::string("PIO.").append(1,'0'+unit),value?"yes":"no");
         break;
      }
   case _4k_EEPROM_with_PIO:
      {
         if (unit<0 || unit>1)
            return;
         writeData(device,std::string("PIO.").append(1,'0'+unit),value?"yes":"no");
         break;
      }
   case microlan_coupler:
      {
         // 1 = Unconditionally off (non-conducting), 2 = Unconditionally on (conducting)
         writeData(device,"control",value?"2":"1");
         break;
      }
   default:
      return;
   }
}

float C1WireByOWFS::GetTemperature(const _t1WireDevice& device) const
{
   std::string readValue=readRawData(std::string(device.filename+"/temperature"));
   if (readValue.empty())
      return -1000.0;
   return static_cast<float>(atof(readValue.c_str()));
}

float C1WireByOWFS::GetHumidity(const _t1WireDevice& device) const
{
   std::string readValue=readRawData(std::string(device.filename+"/humidity"));
   if (readValue.empty())
	   return -1000.0;
   return static_cast<float>(atof(readValue.c_str()));
}

float C1WireByOWFS::GetPressure(const _t1WireDevice& device) const
{
   std::string readValue=readRawData(std::string(device.filename+"/B1-R1-A/pressure"));
   if (readValue.empty())
	   return -1000.0;
   return static_cast<float>(atof(readValue.c_str()));
}

bool C1WireByOWFS::GetLightState(const _t1WireDevice& device,int unit) const
{
   std::string fileName(device.filename);

   switch(device.family)
   {
   case Addresable_Switch:
      {
         fileName.append("/sensed");
         break;
      }
   case dual_addressable_switch_plus_1k_memory:
   case Temperature_IO:
   case dual_channel_addressable_switch:
      {
         if (unit<0 || unit>1)
            return false;
         fileName.append("/sensed.").append(1,'A'+unit);
         break;
      }
   case _8_channel_addressable_switch:
      {
         if (unit<0 || unit>7)
            return false;
         fileName.append("/sensed.").append(1,'0'+unit);
         break;
      }
   case _4k_EEPROM_with_PIO:
      {
         if (unit<0 || unit>1)
            return false;
         fileName.append("/sensed.").append(1,'0'+unit);
         break;
      }
   case microlan_coupler:
      {
         fileName.append("/control");
         break;
      }
   }

   std::string readValue=readRawData(fileName);
   if (readValue.empty())
      return false;

   switch(device.family)
   {
   case Addresable_Switch:
   case dual_addressable_switch_plus_1k_memory:
   case Temperature_IO:
   case dual_channel_addressable_switch:
   case _8_channel_addressable_switch:
   case _4k_EEPROM_with_PIO:
        // Caution : read value correspond to voltage level, inverted from the transistor state
        // We have to invert the read value
      return (readValue!="1");
   case microlan_coupler:
      {
         // 1 = Unconditionally off (non-conducting)
         // 2 = Unconditionally on (conducting)
         int iValue=atoi(readValue.c_str())==2;
         if (iValue!=1 && iValue!=2)
            return false;
         else
            return (iValue==2);
      }
   }
   return false;
}

unsigned int C1WireByOWFS::GetNbChannels(const _t1WireDevice& device) const
{
   std::string readValue=readRawData(std::string(device.filename+"/channels"));
   if (readValue.empty())
      return 0;
   return atoi(readValue.c_str());
}

unsigned long C1WireByOWFS::GetCounter(const _t1WireDevice& device,int unit) const
{
   // Depending on OWFS version, file can be "counter" or "counters". So try both.
   std::string readValue=readRawData(std::string(device.filename+"/counter.").append(1,'A'+unit));
   if (readValue.empty())
      readValue=readRawData(std::string(device.filename+"/counters.").append(1,'A'+unit));
   if (readValue.empty())
	   return 0;
   return (unsigned long)atol(readValue.c_str());
}

int C1WireByOWFS::GetVoltage(const _t1WireDevice& device,int unit) const
{
   std::string fileName(device.filename);

   switch(device.family)
   {
   case smart_battery_monitor:
      {
         static const std::string unitNames[] = { "VAD", "VDD", "vis" };
         if (unit >= (sizeof(unitNames)/sizeof(unitNames[0])))
            return 0;
         fileName.append("/").append(unitNames[unit]);
         break;
      }
   default:
      {
         fileName.append("/volt.").append(1,'A'+unit);
         break;
      }
   }

   std::string readValue=readRawData(fileName);
   if (readValue.empty())
	   return -1000;
   return static_cast<int>((atof(readValue.c_str())*1000.0));
}

float C1WireByOWFS::GetIlluminescence(const _t1WireDevice& device) const
{
   // Both terms "illumination" and "illuminance" are in the OWFS documentation, so try both
   std::string readValue=readRawData(std::string(device.filename+"/S3-R1-A/illuminance"));
   if (readValue.empty())
      readValue=readRawData(std::string(device.filename+"/S3-R1-A/illumination"));
   if (readValue.empty())
	   return -1000.0;
   return (float)(atof(readValue.c_str())*1000.0);
}

bool C1WireByOWFS::IsValidDir(const struct dirent*const de)
{
    // Check dirent type
    if (de->d_type!=DT_DIR)
        return false;

    // Filter system dirs "." and ".."
    std::string dirname = de->d_name;
    if ((dirname==".")||(dirname=="..")||(dirname.size()<3))
        return false;

    // Check dir name format (must be something like xx.xxxxxxxxxxxx where x is hexa)
    if (dirname.size()!=15||
       dirname[2]!='.'||
       !isxdigit(dirname[0])||!isxdigit(dirname[1])||
       !isxdigit(dirname[3])||!isxdigit(dirname[4])||
       !isxdigit(dirname[5])||!isxdigit(dirname[6])||
       !isxdigit(dirname[7])||!isxdigit(dirname[8])||
       !isxdigit(dirname[9])||!isxdigit(dirname[10])||
       !isxdigit(dirname[11])||!isxdigit(dirname[12])||
       !isxdigit(dirname[13])||!isxdigit(dirname[14]))
        return false;

    // The dir is a valid device name
    return true;
}

void C1WireByOWFS::GetDevice(const std::string &inDir, const std::string &dirname, /*out*/_t1WireDevice& device) const
{
    // OWFS device name format, see http://owfs.org/uploads/owfs.1.html#sect30
    // OWFS must be configured to use the default format ff.iiiiiiiiiiii, with :
    // - ff : family (1 byte)
    // - iiiiiiiiiiii : id (6 bytes)

    // Retrieve family from the first 2 chars
    device.family=ToFamily(dirname.substr(0,2));

    // Device Id (6 chars after '.')
    std::string id=dirname.substr(3,3+6*2);
    // OWFS give us the device ID inverted, so reinvert it
    for (int i=0;i<6/2;i++)
    {
		char c;
		int left_position = i * 2;          // Point on first byte
		int right_position=(6-i-1)*2;   // Point on last byte
		c=id[left_position]; id[left_position]=id[right_position]; id[right_position]=c;
		c=id[left_position+1]; id[left_position+1]=id[right_position+1]; id[right_position+1]=c;
    }
    device.devid=id;
    
    // Filename (full path)
    device.filename=inDir;
    device.filename+="/" + dirname;
}

