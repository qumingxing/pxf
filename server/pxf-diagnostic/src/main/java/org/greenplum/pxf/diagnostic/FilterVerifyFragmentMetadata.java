package org.greenplum.pxf.diagnostic;

import lombok.AllArgsConstructor;
import lombok.Getter;
import lombok.NoArgsConstructor;
import org.greenplum.pxf.api.utilities.FragmentMetadata;

@NoArgsConstructor
@AllArgsConstructor
public class FilterVerifyFragmentMetadata implements FragmentMetadata {

    @Getter
    private String filter;
}
