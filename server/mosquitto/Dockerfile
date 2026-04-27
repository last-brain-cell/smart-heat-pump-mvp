FROM eclipse-mosquitto:2

COPY config/mosquitto.conf /mosquitto/config/mosquitto.conf

COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh && \
    chown -R mosquitto:mosquitto /mosquitto/config

ENTRYPOINT ["/entrypoint.sh"]
CMD ["/usr/sbin/mosquitto", "-c", "/mosquitto/config/mosquitto.conf"]
