#include "op/file_download_task.h"

#include <stdint.h>
#include <string.h>

#include <map>

namespace qcloud_cos{

FileDownTask::FileDownTask(const std::string& full_url,
                           const std::map<std::string, std::string>& headers,
                           const std::map<std::string, std::string>& params,
                           uint64_t conn_timeout_in_ms,
                           uint64_t recv_timeout_in_ms,
                           uint64_t offset,
                           unsigned char* pbuf,
                           const size_t data_len)
    : m_full_url(full_url), m_headers(headers), m_params(params),
      m_conn_timeout_in_ms(conn_timeout_in_ms),
      m_recv_timeout_in_ms(recv_timeout_in_ms),
      m_offset(offset), m_data_buf_ptr(pbuf),
      m_data_len(data_len), m_resp(""), m_is_task_success(false), m_real_down_len(0), m_target_size(0) {
}

void FileDownTask::Run() {
    m_resp = "";
    m_is_task_success = false;
    DownTask();
}

void FileDownTask::SetDownParams(unsigned char* pbuf, size_t data_len, uint64_t offset, uint64_t target_size) {
    m_data_buf_ptr = pbuf;
    m_data_len  = data_len;
    m_offset = offset;
    m_target_size = target_size;
}

size_t FileDownTask::GetDownLoadLen() {
    return m_real_down_len;
}

bool FileDownTask::IsTaskSuccess() {
    return m_is_task_success;
}

std::string FileDownTask::GetTaskResp() {
    return m_resp;
}

int FileDownTask::GetHttpStatus() {
    return m_http_status;
}

std::map<std::string, std::string> FileDownTask::GetRespHeaders() {
    return m_resp_headers;
}

void FileDownTask::DownTask() {
    char range_head[128];
    memset(range_head, 0, sizeof(range_head));
    snprintf(range_head, sizeof(range_head), "bytes=%llu-%llu",
             m_offset, (m_offset + m_data_len - 1));

    // 增加Range头域，避免大文件时将整个文件下载
    m_headers["Range"] = range_head;

    m_http_status = HttpSender::SendRequest("GET", m_full_url, m_params, m_headers,
                                            "", m_conn_timeout_in_ms, m_recv_timeout_in_ms,
                                            &m_resp_headers, &m_resp, &m_err_msg);

    //当实际长度小于请求的数据长度时httpcode为206
    if (m_http_status != 200 && m_http_status != 206) {
        SDK_LOG_ERR("FileDownload: url(%s) fail, httpcode:%d, resp: %s",
                    m_full_url.c_str(), m_http_status, m_resp.c_str());
        m_is_task_success = false;
        m_real_down_len = 0;
        return;
    }

    size_t buf_max_size = m_data_len;
    size_t len = MIN(m_resp.length(), buf_max_size);
    memcpy(m_data_buf_ptr, m_resp.c_str(), len);
    m_real_down_len = len;

    // Must notice the receive timeout in Poco socket in SendRequest()
    // The receiveResponse does not throw the expection of timeout,
    // so there check the each part size outside, in case of the file incomplete.
    if (len != m_target_size) {
        SDK_LOG_ERR("FileDownload: url(%s) fail, might be reseted connection, offset:%lld, received:%d, expected: %lld",
                    m_full_url.c_str(), m_offset, len, m_target_size);
        m_is_task_success = false;
        // Clear the resp 
        m_resp = "";
        return;
    }

    m_is_task_success = true;
    m_resp = "";
    return;
}

} // namespace qcloud_cos
