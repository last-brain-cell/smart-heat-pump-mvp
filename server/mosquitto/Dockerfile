FROM eclipse-mosquitto:2

COPY config/mosquitto.conf /mosquitto/config/mosquitto.conf

COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh && \
    touch /mosquitto/config/passwd && \
    chown mosquitto:mosquitto /mosquitto/config/passwd

ENTRYPOINT ["/entrypoint.sh"]
CMD ["/usr/sbin/mosquitto", "-c", "/mosquitto/config/mosquitto.conf"]
