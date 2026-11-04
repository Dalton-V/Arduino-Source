/*  DirectShow Device Catalog
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifdef _WIN32
#include "Common/Cpp/Concurrency/AsyncTask.h"
#include "Common/Cpp/Concurrency/ConditionVariable.h"
#include "Common/Cpp/Concurrency/Mutex.h"
#include "CommonFramework/Tools/GlobalThreadPools.h"
#include "DirectShowCatalog.h"
namespace PokemonAutomation::DirectShow{
namespace{
class Catalog{
public:
    Catalog(){
        m_task = GlobalThreadPools::unlimited_normal().dispatch([this]{
            while (true){
                CatalogSnapshot refreshed = enumerate_video_devices();
                std::unique_lock<Mutex> lock(m_lock);
                if (refreshed.error.empty()){
                    m_snapshot = std::move(refreshed);
                }else{
                    m_snapshot.error = std::move(refreshed.error);
                }
                if (m_cv.wait_for(lock, std::chrono::seconds(2), [this]{ return m_stopping; })){
                    break;
                }
            }
        });
    }
    ~Catalog(){ stop(); }
    void stop(){
        {
            std::lock_guard<Mutex> lock(m_lock);
            m_stopping = true;
            m_cv.notify_all();
        }
        m_task.wait_and_ignore_exceptions();
    }
    CatalogSnapshot snapshot(){
        std::lock_guard<Mutex> lock(m_lock);
        return m_snapshot;
    }
private:
    Mutex m_lock;
    ConditionVariable m_cv;
    bool m_stopping = false;
    CatalogSnapshot m_snapshot;
    AsyncTask m_task;
};
Catalog& catalog(){
    static Catalog instance;
    return instance;
}
}
CatalogSnapshot cached_video_devices(){
    return catalog().snapshot();
}
void stop_video_catalog(){
    catalog().stop();
}
}
#endif
