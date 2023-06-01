<?php
$datetime_disabled = false;
	for ($r=0; $r<4; $r++) {
?>
                
                <div class="pure-u-1 pure-u-md-1-2">
                    <div class="relay">
                        <form id="relay_<?php echo $r; ?>" class="pure-form" action="/setuprelays" method="POST">
                            <fieldset class="pure-group">
                                <input type="hidden" name="url" value="/setup.html#relay_<?php echo $r; ?>" />
                                <input type="hidden" name="r" value="<?php echo $r; ?>" />
                                <input type="text" name="name" placeholder="Nombre" />
                            </fieldset>
                            <fieldset>
                                <label for="hide_<?php echo $r; ?>">Ocultar</label>
                                <input type="checkbox" id="hide_<?php echo $r; ?>" name="hide" value="1" />
                            </fieldset>
                            <fieldset class="pure-group">
                                <select name="tr" id="activacion_<?php echo $r; ?>">
                                    <option value="0">Inteligente</option>
<?php
        if (!$datetime_disabled) {
?>
                                    <option value="1">Temporizador</option>
<?php
        }
?>
                                    <option value="2">Manual</option>
                                    <option value="3">Crepuscular</option>
                                    <option value="4">Movimiento</option>
                                    <option value="5">Sonido</option>
                                </select>
                            </fieldset>
                            <fieldset class="pure-group">
                                <label for="light_<?php echo $r; ?>">Luz mínima para funcionar:</label>
                                <input type="number" name="light" id="light_<?php echo $r; ?>" min="0" max="100"><!-- <span class="pure-form-message-inline">%</span> -->
                            </fieldset>
<?php
        if (!$datetime_disabled) {
?>
                            <fieldset class="pure-group">
                                <label>Horario de activación desde/hasta:</label>
                                <input type="time" name="from" id="from_<?php echo $r; ?>">
                                <input type="time" name="to" id="to_<?php echo $r; ?>">
                            </fieldset>
<?php
        }
?>
                            <fieldset class="pure-group">
                                <label>Espera min/max antes de encender (en milisegundos)</label>
                                <input type="number" name="dmin" id="dmin_<?php echo $r; ?>">
                                <input type="number" name="dmax" id="dmax_<?php echo $r; ?>">
                            </fieldset>
                            <fieldset class="pure-group">
                                <label>Permanencia min/max antes de apagar (en segundos)</label>
                                <input type="number" name="smin" id="smin_<?php echo $r; ?>">
                                <input type="number" name="smax" id="smax_<?php echo $r; ?>">
                            </fieldset>
                            <fieldset class="pure-group">
                                <button type="submit" class="pure-button pure-button-primary">Guardar</button>
                            </fieldset>
                        </form>
                    </div>
                </div>
                
<?php
	}
?>
