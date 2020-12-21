package org.greenplum.pxf.service;

import org.greenplum.pxf.api.configuration.PxfServerProperties;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.ListableBeanFactory;
import org.springframework.beans.factory.ObjectProvider;
import org.springframework.boot.autoconfigure.task.TaskExecutionProperties;
import org.springframework.boot.context.properties.EnableConfigurationProperties;
import org.springframework.boot.task.TaskExecutorBuilder;
import org.springframework.boot.task.TaskExecutorCustomizer;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.core.task.AsyncTaskExecutor;
import org.springframework.core.task.TaskDecorator;
import org.springframework.core.task.TaskExecutor;
import org.springframework.scheduling.annotation.AsyncAnnotationBeanPostProcessor;
import org.springframework.scheduling.concurrent.ThreadPoolTaskExecutor;
import org.springframework.web.servlet.config.annotation.AsyncSupportConfigurer;
import org.springframework.web.servlet.config.annotation.WebMvcConfigurer;

import java.time.Duration;
import java.util.concurrent.Executors;

/**
 * Configures the {@link AsyncTaskExecutor} for tasks that will stream data to
 * clients
 */
@Configuration
@EnableConfigurationProperties(PxfServerProperties.class)
public class PxfConfiguration implements WebMvcConfigurer {

    private static final Logger LOG = LoggerFactory.getLogger(PxfConfiguration.class);

    /**
     * Bean name of PXF's {@link TaskExecutor}.
     */
    public static final String PXF_RESPONSE_STREAM_TASK_EXECUTOR = "pxfResponseStreamTaskExecutor";

    /**
     * Bean name of PXF's {@link TaskExecutor} for producer tasks.
     */
    public static final String PXF_PRODUCER_TASK_EXECUTOR = "pxfProducerTaskExecutor";

    /**
     * Bean name of PXF's {@link TaskExecutor} for tuple tasks.
     */
    public static final String PXF_TUPLE_TASK_EXECUTOR = "pxfTupleTaskExecutor";

    private final ListableBeanFactory beanFactory;

    /**
     * Constructs a PXF Configuration object with the provided
     * {@link ListableBeanFactory}
     *
     * @param beanFactory the beanFactory
     */
    public PxfConfiguration(ListableBeanFactory beanFactory) {
        this.beanFactory = beanFactory;
    }

    /**
     * Configures the TaskExecutor to be used for async requests (i.e. Bridge
     * Read).
     */
    @Override
    public void configureAsyncSupport(AsyncSupportConfigurer configurer) {
        AsyncTaskExecutor taskExecutor = (AsyncTaskExecutor) this.beanFactory
                .getBean(PXF_RESPONSE_STREAM_TASK_EXECUTOR);
        configurer.setTaskExecutor(taskExecutor);
    }

    /**
     * Configures and builds the {@link ThreadPoolTaskExecutor}
     *
     * @return the {@link ThreadPoolTaskExecutor}
     */
    @Bean(name = {PXF_RESPONSE_STREAM_TASK_EXECUTOR,
            AsyncAnnotationBeanPostProcessor.DEFAULT_TASK_EXECUTOR_BEAN_NAME})
    public ThreadPoolTaskExecutor pxfApplicationTaskExecutor(PxfServerProperties pxfServerProperties,
                                                             ObjectProvider<TaskExecutorCustomizer> taskExecutorCustomizers,
                                                             ObjectProvider<TaskDecorator> taskDecorator) {

        TaskExecutionProperties properties = pxfServerProperties.getTask();
        TaskExecutionProperties.Pool pool = properties.getPool();
        TaskExecutorBuilder builder = new TaskExecutorBuilder();
        builder = builder.queueCapacity(pool.getQueueCapacity());
        builder = builder.corePoolSize(pool.getCoreSize());
        builder = builder.maxPoolSize(pool.getMaxSize());
        builder = builder.allowCoreThreadTimeOut(pool.isAllowCoreThreadTimeout());
        builder = builder.keepAlive(pool.getKeepAlive());
        TaskExecutionProperties.Shutdown shutdown = properties.getShutdown();
        builder = builder.awaitTermination(shutdown.isAwaitTermination());
        builder = builder.awaitTerminationPeriod(shutdown.getAwaitTerminationPeriod());
        builder = builder.threadNamePrefix(properties.getThreadNamePrefix());
        builder = builder.customizers(taskExecutorCustomizers.orderedStream()::iterator);
        builder = builder.taskDecorator(taskDecorator.getIfUnique());

        LOG.debug("Initializing PXF ThreadPoolTaskExecutor with prefix={}. " +
                        "Pool options: " +
                        "queue capacity={}, core size={}, max size={}, " +
                        "allow core thread timeout={}, keep alive={}. " +
                        "Shutdown options: await termination={}, await " +
                        "termination period={}.",
                properties.getThreadNamePrefix(),
                pool.getQueueCapacity(),
                pool.getCoreSize(),
                pool.getMaxSize(),
                pool.isAllowCoreThreadTimeout(),
                pool.getKeepAlive(),
                shutdown.isAwaitTermination(),
                shutdown.getAwaitTerminationPeriod());

        return builder.build(PxfThreadPoolTaskExecutor.class);
    }

    @Bean(name = PXF_PRODUCER_TASK_EXECUTOR)
    public ThreadPoolTaskExecutor pxfProducerTaskExecutor() {
        TaskExecutorBuilder builder = new TaskExecutorBuilder();
        builder = builder.queueCapacity(0);
        builder = builder.corePoolSize(8);
        builder = builder.maxPoolSize(Integer.MAX_VALUE);
        builder = builder.allowCoreThreadTimeOut(false);
        builder = builder.keepAlive(Duration.ofSeconds(60));
        builder = builder.threadNamePrefix("pxf-producer-");
        return builder.build();
    }

    @Bean(name = PXF_TUPLE_TASK_EXECUTOR)
    public ThreadPoolTaskExecutor pxfTupleTaskExecutor() {
        TaskExecutorBuilder builder = new TaskExecutorBuilder();
        builder = builder.queueCapacity(0);
        builder = builder.corePoolSize(8);
        builder = builder.maxPoolSize(Integer.MAX_VALUE);
        builder = builder.allowCoreThreadTimeOut(false);
        builder = builder.keepAlive(Duration.ofSeconds(60));
        builder = builder.threadNamePrefix("pxf-tuple-executor-");
        return builder.build();
    }
}
