package org.greenplum.pxf.service.rest;

import org.greenplum.pxf.api.model.QuerySession;
import org.greenplum.pxf.service.QuerySessionManager;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.util.MultiValueMap;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestHeader;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.servlet.mvc.method.annotation.StreamingResponseBody;

/**
 * This controller handles requests for scans on external systems. The external
 * scan will produce tuples that will then be serialized to the output stream
 * so it can then be processed by the client.
 */
@RestController
@RequestMapping("/pxf/" + Version.PXF_PROTOCOL_VERSION)
public class ScanController {

    private final QuerySessionManager querySessionManager;

    public ScanController(QuerySessionManager querySessionManager) {
        this.querySessionManager = querySessionManager;
    }

    @GetMapping("/scan")
    public ResponseEntity<StreamingResponseBody> scan(
            @RequestHeader MultiValueMap<String, String> headers)
            throws Throwable {

        // Get the query session
        // QuerySession has the processor, the RequestContext, state of the
        // query, among other information
        QuerySession querySession = querySessionManager.get(headers);
        
        if (querySession == null) {
            return new ResponseEntity<>(HttpStatus.OK);
        }

        // Create a streaming class which will consume tuples from the
        // querySession object and serialize them to the output stream
        StreamingResponseBody response = new ScanResponse(querySession);

        // returns the response to the client
        return new ResponseEntity<>(response, HttpStatus.OK);
    }

}